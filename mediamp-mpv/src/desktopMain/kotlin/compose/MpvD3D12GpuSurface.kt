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
import org.jetbrains.skia.Bitmap
import org.jetbrains.skia.ColorSpace
import org.jetbrains.skia.DirectContext
import org.jetbrains.skia.Image
import org.jetbrains.skia.ImageInfo
import org.jetbrains.skia.Surface
import org.jetbrains.skia.SurfaceColorFormat
import org.jetbrains.skia.SurfaceOrigin
import org.openani.mediamp.InternalMediampApi
import org.openani.mediamp.mpv.MPVHandle
import org.openani.mediamp.mpv.MpvMediampPlayer
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

    // If D3D12 path failed, fall back to async readback
    val failed by renderer.failed
    if (failed) {
        println("[D3D12Gpu] D3D12 path failed, falling back to async readback")
        MpvMediampPlayerSurface(mediampPlayer, modifier)
        return
    }

    // Drive continuous redraws at display refresh rate.
    // The render thread produces frames independently; the Canvas just reads whatever is current.
    var redrawTick by remember { mutableStateOf(0L) }
    LaunchedEffect(renderer) {
        while (true) {
            withFrameNanos { redrawTick++ }
        }
    }

    Canvas(modifier) {
        redrawTick // establish state subscription to trigger recomposition each frame
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

    // Current frame state (CPU-backed image, safe to draw on any Canvas)
    @Volatile private var currentImage: Image? = null
    @Volatile private var imageWidth = 0
    @Volatile private var imageHeight = 0

    // Cached CPU readback resources
    private var readbackBitmap: Bitmap? = null

    val failed = mutableStateOf(false)

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
        println("[D3D12Gpu] renderLoop started")

        // CRITICAL: Create ANGLE render context FIRST, before waiting for Compose device.
        // mpv's video output (vo=libmpv) needs the render context to be available immediately.
        // If we delay, mpv fails with "[vo/libmpv:fatal] No render context set."
        val angleOk = handle.createAngleRenderContext(1920, 1080)
        if (!angleOk) {
            println("[D3D12Gpu] Failed to create ANGLE render context")
            failed.value = true
            return
        }
        println("[D3D12Gpu] ANGLE render context created")

        // Now wait for Compose's D3D12 device to become available
        // (SkiaLayer may not be initialized yet when we start)
        println("[D3D12Gpu] Waiting for Compose D3D12 device...")
        var device: SkikoD3D12Access.ComposeD3D12Device? = null
        for (attempt in 1..50) {
            val window = findComposeWindow()
            if (window != null) {
                device = SkikoD3D12Access.getComposeD3D12Device(window)
            }
            if (device != null) break
            if (!running) {
                handle.destroyAngleRenderContext()
                return
            }
            println("[D3D12Gpu] Waiting for Compose D3D12 device... attempt $attempt")
            Thread.sleep(200)
        }

        if (device == null) {
            println("[D3D12Gpu] FAILED: Could not get Compose D3D12 device after 50 attempts")
            println("[D3D12Gpu] Falling back: Compose may be using software or OpenGL backend")
            handle.destroyAngleRenderContext()
            failed.value = true
            return
        }

        composeDevice = device
        val devicePtr = device.devicePtr
        println("[D3D12Gpu] Got Compose D3D12 device: device=$devicePtr, hasContext=${device.directContext != null}")

        // Always create our own DirectContext — Skiko's is not thread-safe for our render thread.
        // devicePtr from Skiko is a struct pointer, NOT a raw ID3D12Device*.
        // DirectContext.makeDirect3D needs the raw device pointer, so we extract it via JNI.
        val rawDevicePtr = MPVHandle.extractD3D12DevicePtr(devicePtr)
        if (rawDevicePtr == 0L) {
            println("[D3D12Gpu] Failed to extract raw ID3D12Device from Skiko struct")
            handle.destroyAngleRenderContext()
            failed.value = true
            return
        }
        println("[D3D12Gpu] Extracted raw ID3D12Device: $rawDevicePtr")

        val adapterPtr = device.adapterPtr.takeIf { it != 0L }
            ?: MPVHandle.getD3D12DefaultAdapter()
        val queuePtr = device.queuePtr.takeIf { it != 0L }
            ?: MPVHandle.createD3D12CommandQueue(devicePtr)
        if (adapterPtr == 0L || queuePtr == 0L) {
            println("[D3D12Gpu] Failed to get adapter/queue for DirectContext")
            handle.destroyAngleRenderContext()
            failed.value = true
            return
        }
        val directContext = DirectContext.makeDirect3D(adapterPtr, rawDevicePtr, queuePtr)
        println("[D3D12Gpu] Created own DirectContext: $directContext")

        // Cache D3D12 resources per shared handle to avoid release/re-open every frame.
        // With double-buffered textures, there are only 2 unique handles.
        val d3d12TextureCache = HashMap<Long, Long>()         // sharedHandle → d3d12TexturePtr
        val backendRTCache = HashMap<Long, BackendRenderTarget>()  // sharedHandle → BackendRenderTarget

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

                // Get or create D3D12 resources for this handle
                var rt = backendRTCache[sharedHandle]
                if (rt == null) {
                    val d3d12TexturePtr = MPVHandle.openSharedTextureOnD3D12(devicePtr, sharedHandle)
                    if (d3d12TexturePtr == 0L) {
                        println("[D3D12Gpu] Failed to open shared texture on Compose D3D12 device")
                        continue
                    }

                    // Create BackendRenderTarget from D3D12 texture
                    // DXGI_FORMAT_B8G8R8A8_UNORM = 87 (ANGLE default)
                    rt = BackendRenderTarget.makeDirect3D(
                        1920, 1080,
                        d3d12TexturePtr,
                        87, // DXGI_FORMAT_B8G8R8A8_UNORM
                        1,  // sampleCnt
                        1   // levelCnt
                    )
                    if (rt == null) {
                        MPVHandle.releaseD3D12Resource(d3d12TexturePtr)
                        println("[D3D12Gpu] Failed to create BackendRenderTarget")
                        continue
                    }

                    d3d12TextureCache[sharedHandle] = d3d12TexturePtr
                    backendRTCache[sharedHandle] = rt
                    println("[D3D12Gpu] Imported D3D12 texture: handle=$sharedHandle, texture=$d3d12TexturePtr")
                }

                // Create Surface from BackendRenderTarget using our DirectContext
                // ANGLE uses BGRA by default, so we match that format
                val surface = Surface.makeFromBackendRenderTarget(
                    directContext,
                    rt,
                    SurfaceOrigin.TOP_LEFT,
                    SurfaceColorFormat.BGRA_8888,
                    ColorSpace.sRGB,
                )

                if (surface != null) {
                    // Flush Skia's D3D12 pipeline and wait for GPU to ensure
                    // synchronization with D3D11 writes from ANGLE
                    directContext.flushAndSubmit(surface, true)

                    // Try reading pixels from Surface directly (before close)
                    val w = 1920
                    val h = 1080
                    val bmp = readbackBitmap ?: Bitmap().also { readbackBitmap = it }
                    bmp.allocPixels(ImageInfo.makeN32Premul(w, h))

                    // Method 1: Surface.readPixels
                    var readOk = surface.readPixels(bmp, 0, 0)
                    if (!readOk) {
                        // Method 2: Snapshot → Image.readPixels
                        val gpuImage = surface.makeImageSnapshot()
                        if (gpuImage != null) {
                            readOk = gpuImage.readPixels(bmp, 0, 0)
                            gpuImage.close()
                        }
                    }
                    surface.close()

                    if (readOk) {
                        val cpuImage = Image.makeFromBitmap(bmp)
                        val oldImage = currentImage
                        currentImage = cpuImage
                        imageWidth = w
                        imageHeight = h
                        oldImage?.close()

                        if (renderedFrames <= 3 || renderedFrames % 150 == 0L) {
                            println("[D3D12Gpu] CPU readback OK: ${w}x${h}")
                        }
                    } else {
                        if (renderedFrames <= 3) println("[D3D12Gpu] readPixels failed (both Surface and Image)")
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

        // Cleanup cached resources
        backendRTCache.values.forEach { it.close() }
        d3d12TextureCache.values.forEach { MPVHandle.releaseD3D12Resource(it) }
        directContext.close()
        MPVHandle.releaseD3D12Resource(rawDevicePtr)  // Release the extra AddRef from extractD3D12DevicePtr
        handle.destroyAngleRenderContext()
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
            t.join(5000) // Wait for render thread to finish before cleaning up resources
        }
        renderThread = null
        currentImage?.close()
        currentImage = null
        readbackBitmap?.close()
        readbackBitmap = null
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
