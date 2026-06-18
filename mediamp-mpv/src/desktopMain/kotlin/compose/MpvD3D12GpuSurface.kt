/*
 * P3: GPU direct pass-through using Compose's own D3D12 device.
 *
 * Instead of creating our own D3D12 device (which produces images incompatible
 * with Compose's Canvas), this renderer extracts Compose/Skiko's D3D12 device
 * via reflection and imports ANGLE's shared texture on THAT device.
 *
 * Pipeline: mpv ANGLE → D3D11 shared texture → import on Compose's D3D12 device
 *           → BackendRenderTarget → Surface → makeImageSnapshot → Compose canvas.drawImage
 */

package org.openani.mediamp.mpv.compose

import androidx.compose.foundation.Canvas
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.graphics.drawscope.drawIntoCanvas
import androidx.compose.ui.util.fastCoerceAtLeast
import org.jetbrains.skia.BackendRenderTarget
import org.jetbrains.skia.ColorSpace
import org.jetbrains.skia.DirectContext
import org.jetbrains.skia.Image
import org.jetbrains.skia.Surface
import org.jetbrains.skia.SurfaceColorFormat
import org.jetbrains.skia.SurfaceOrigin
import org.openani.mediamp.InternalMediampApi
import org.openani.mediamp.mpv.MPVHandle
import org.openani.mediamp.mpv.MpvMediampPlayer
import javax.swing.SwingUtilities
import kotlin.math.min
import kotlin.math.roundToInt

@OptIn(InternalMediampApi::class)
@Composable
public fun MpvD3D12GpuSurface(
    mediampPlayer: MpvMediampPlayer,
    modifier: Modifier = Modifier,
) {
    val renderer = remember { D3D12GpuRenderer(mediampPlayer) }

    DisposableEffect(mediampPlayer) {
        renderer.start()
        onDispose { renderer.stop() }
    }

    val frameTrigger by renderer.frameTrigger

    Canvas(modifier) {
        frameTrigger // read to establish subscription
        drawIntoCanvas { canvas ->
            renderer.drawTo(canvas.nativeCanvas, Size(size.width, size.height))
        }
    }
}

@OptIn(InternalMediampApi::class)
private class D3D12GpuRenderer(
    private val player: MpvMediampPlayer,
) {
    private val handle: MPVHandle = player.impl

    // Compose's D3D12 device (obtained via reflection)
    private var composeDevice: SkikoD3D12Access.ComposeD3D12Device? = null

    // Current frame state
    @Volatile private var currentImage: Image? = null
    @Volatile private var imageWidth = 0
    @Volatile private var imageHeight = 0

    val frameTrigger = mutableStateOf(0L)

    private var renderThread: Thread? = null
    @Volatile private var running = false

    fun start() {
        if (running) return
        running = true
        println("[D3D12Gpu] start() called")

        renderThread = Thread({ renderLoop() }, "mpv-d3d12-gpu").apply {
            isDaemon = true
            start()
        }
    }

    private fun renderLoop() {
        println("[D3D12Gpu] renderLoop started, attempting to get Compose D3D12 device...")

        // Wait for Compose's D3D12 device to become available
        // (SkiaLayer may not be initialized yet when we start)
        var device: SkikoD3D12Access.ComposeD3D12Device? = null
        for (attempt in 1..50) {
            // Find the active window from AWT hierarchy
            val window = findComposeWindow()
            if (window != null) {
                device = SkikoD3D12Access.getComposeD3D12Device(window)
            }
            if (device != null) break
            if (!running) return
            println("[D3D12Gpu] Waiting for Compose D3D12 device... attempt $attempt")
            Thread.sleep(200)
        }

        if (device == null) {
            println("[D3D12Gpu] FAILED: Could not get Compose D3D12 device after 50 attempts")
            println("[D3D12Gpu] Falling back: Compose may be using software or OpenGL backend")
            return
        }

        composeDevice = device
        val devicePtr = device.devicePtr
        println("[D3D12Gpu] Got Compose D3D12 device: device=$devicePtr, hasContext=${device.directContext != null}")

        // Use Skiko's DirectContext if available, otherwise create our own
        val ownContext: DirectContext?
        val directContext: DirectContext
        if (device.directContext != null) {
            directContext = device.directContext
            ownContext = null
            println("[D3D12Gpu] Using Skiko's DirectContext: $directContext")
        } else {
            val adapterPtr = device.adapterPtr
            val queuePtr = device.queuePtr
            if (adapterPtr == 0L || queuePtr == 0L) {
                println("[D3D12Gpu] No adapter/queue available for creating DirectContext")
                return
            }
            val ctx = DirectContext.makeDirect3D(adapterPtr, devicePtr, queuePtr)
            ownContext = ctx
            directContext = ctx
            println("[D3D12Gpu] Created own DirectContext: $directContext")
        }

        // Create ANGLE render context on this thread
        val angleOk = handle.createAngleRenderContext(1920, 1080)
        if (!angleOk) {
            println("[D3D12Gpu] Failed to create ANGLE render context")
            directContext.close()
            return
        }
        println("[D3D12Gpu] ANGLE render context created")

        // Import shared texture on Compose's D3D12 device
        var d3d12TexturePtr = 0L
        var cachedSharedHandle = 0L
        var backendRT: BackendRenderTarget? = null

        var renderedFrames = 0L
        var lastStatTime = System.nanoTime()

        while (running) {
            try {
                handle.waitForFrame()
                if (!running) break

                // Render mpv frame via ANGLE (D3D11 → shared texture)
                val rendered = handle.renderAngleFrame()
                if (!rendered) continue

                renderedFrames++

                // Get the shared HANDLE of the current read texture
                val sharedHandle = handle.getAngleSharedTextureHandle()
                if (sharedHandle == 0L) continue

                // Import shared texture on Compose's D3D12 device if handle changed
                if (sharedHandle != cachedSharedHandle) {
                    // Release old D3D12 texture
                    if (d3d12TexturePtr != 0L) {
                        MPVHandle.releaseD3D12Resource(d3d12TexturePtr)
                        d3d12TexturePtr = 0L
                    }
                    // Release old backend render target
                    backendRT?.close()
                    backendRT = null

                    // Open shared texture on Compose's D3D12 device
                    d3d12TexturePtr = MPVHandle.openSharedTextureOnD3D12(devicePtr, sharedHandle)
                    if (d3d12TexturePtr == 0L) {
                        println("[D3D12Gpu] Failed to open shared texture on Compose D3D12 device")
                        continue
                    }

                    // Create BackendRenderTarget from D3D12 texture
                    // DXGI_FORMAT_R8G8B8A8_UNORM = 28
                    backendRT = BackendRenderTarget.makeDirect3D(
                        1920, 1080,
                        d3d12TexturePtr,
                        28, // DXGI_FORMAT_R8G8B8A8_UNORM
                        1,  // sampleCnt
                        1   // levelCnt
                    )

                    cachedSharedHandle = sharedHandle
                    println("[D3D12Gpu] Imported D3D12 texture on Compose device: handle=$sharedHandle, texture=$d3d12TexturePtr")
                }

                val rt = backendRT ?: continue

                // Create Surface from BackendRenderTarget using our DirectContext
                // (created from Compose's device, so resources are compatible)
                val surface = Surface.makeFromBackendRenderTarget(
                    directContext,
                    rt,
                    SurfaceOrigin.TOP_LEFT,
                    SurfaceColorFormat.RGBA_8888,
                    ColorSpace.sRGB,
                )

                if (surface != null) {
                    // Snapshot → Image (backed by texture on Compose's device)
                    val image = surface.makeImageSnapshot()
                    surface.close()

                    if (image != null) {
                        // Close old image
                        currentImage?.close()
                        currentImage = image
                        imageWidth = image.width
                        imageHeight = image.height

                        if (renderedFrames <= 3 || renderedFrames % 150 == 0L) {
                            println("[D3D12Gpu] Snapshot OK: ${image.width}x${image.height}, image=$image")
                        }

                        SwingUtilities.invokeLater {
                            frameTrigger.value = System.nanoTime()
                        }
                    } else {
                        println("[D3D12Gpu] makeImageSnapshot returned null")
                    }
                } else {
                    println("[D3D12Gpu] makeFromBackendRenderTarget returned null")
                }

                // Stats
                val now = System.nanoTime()
                val elapsed = now - lastStatTime
                if (elapsed >= 5_000_000_000L) {
                    val fps = renderedFrames * 1_000_000_000.0 / elapsed
                    println("[D3D12Gpu] FPS: ${"%.1f".format(fps)}, rendered=$renderedFrames")
                    renderedFrames = 0
                    lastStatTime = now
                }
            } catch (e: InterruptedException) {
                break
            } catch (e: Exception) {
                println("[D3D12Gpu] Exception: ${e.message}")
                e.printStackTrace()
                Thread.sleep(100)
            }
        }

        // Cleanup
        backendRT?.close()
        ownContext?.close() // Only close if we created it (not Skiko's)
        if (d3d12TexturePtr != 0L) MPVHandle.releaseD3D12Resource(d3d12TexturePtr)
        handle.destroyAngleRenderContext()
        // Don't release device/adapter/queue/context - they belong to Compose
    }

    private var drawCount = 0L

    fun drawTo(canvas: org.jetbrains.skia.Canvas, canvasSize: Size) {
        val image = currentImage ?: run {
            if (drawCount % 300 == 0L) println("[D3D12Gpu] drawTo: no image")
            drawCount++
            return
        }
        val w = imageWidth
        val h = imageHeight
        if (w <= 0 || h <= 0) return

        if (drawCount <= 3 || drawCount % 300 == 0L) {
            println("[D3D12Gpu] drawTo: image=${w}x${h}, canvas=${canvasSize.width}x${canvasSize.height}, canvas=$canvas")
        }
        drawCount++

        val scale = min(
            canvasSize.width / w.toFloat(),
            canvasSize.height / h.toFloat(),
        )
        val scaledW = (w * scale).roundToInt()
        val scaledH = (h * scale).roundToInt()
        val offsetX = ((canvasSize.width - scaledW) / 2f).fastCoerceAtLeast(0f)
        val offsetY = ((canvasSize.height - scaledH) / 2f).fastCoerceAtLeast(0f)

        canvas.drawImageRect(
            image,
            org.jetbrains.skia.Rect.makeWH(w.toFloat(), h.toFloat()),
            org.jetbrains.skia.Rect.makeXYWH(offsetX, offsetY, scaledW.toFloat(), scaledH.toFloat()),
        )
    }

    fun stop() {
        running = false
        renderThread?.let { t ->
            t.interrupt()
            if (t.isAlive) {
                try { t.isDaemon = true } catch (_: IllegalThreadStateException) {}
            }
        }
        renderThread = null
        currentImage?.close()
        currentImage = null
    }

    /**
     * Find the active Compose window from the AWT hierarchy.
     * Searches for a Window that contains a SkiaLayer component.
     */
    private var dumpedComponents = false

    private fun findComposeWindow(): java.awt.Window? {
        val windows = java.awt.Window.getWindows()
        if (windows.isEmpty()) {
            println("[D3D12Gpu] findComposeWindow: no windows found")
            return null
        }
        for (window in windows) {
            val showing = window.isShowing
            val hasSkia = if (showing) containsSkiaLayer(window) else false
            if (showing) {
                println("[D3D12Gpu] findComposeWindow: window=${window.javaClass.name}, showing=$showing, hasSkiaLayer=$hasSkia")
                if (!dumpedComponents && showing) {
                    dumpedComponents = true
                    dumpComponents(window, 0)
                }
            }
            if (showing && hasSkia) {
                return window
            }
        }
        return null
    }

    private fun dumpComponents(container: java.awt.Container, depth: Int) {
        val indent = "  ".repeat(depth)
        for (component in container.components) {
            println("[D3D12Gpu] ${indent}${component.javaClass.name}")
            if (depth < 4 && component is java.awt.Container) {
                dumpComponents(component, depth + 1)
            }
        }
    }

    private fun containsSkiaLayer(container: java.awt.Container): Boolean {
        for (component in container.components) {
            val name = component.javaClass.name
            if (name.contains("SkiaLayer") || name.contains("skiko")) {
                println("[D3D12Gpu] Found skiko component: $name")
                return true
            }
            if (component is java.awt.Container && containsSkiaLayer(component)) {
                return true
            }
        }
        return false
    }
}
