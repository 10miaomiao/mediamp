/*
 * P3: GPU direct pass-through using Compose's own D3D12 device and DirectContext.
 *
 * Key insight: GPU images are bound to the DirectContext that created them.
 * To draw on Compose's canvas, we must create the Surface/Image using
 * Compose's own DirectContext, which is only accessible on the draw thread.
 *
 * Pipeline:
 *   Render thread: mpv ANGLE → D3D11 shared texture → store shared handle
 *   Draw thread:   open shared handle on D3D12 → BackendRenderTarget → Surface
 *                  → makeImageSnapshot → canvas.drawImage (all on Compose's DirectContext)
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

    // Shared handle from the latest rendered frame (set by render thread, read by draw thread)
    @Volatile private var latestSharedHandle: Long = 0L
    @Volatile private var frameWidth = 0
    @Volatile private var frameHeight = 0

    // Compose's DirectContext (obtained on draw thread)
    private var composeDirectContext: DirectContext? = null

    // D3D12 device pointer (raw ID3D12Device*)
    private var rawDevicePtr: Long = 0L
    private var queuePtr: Long = 0L
    private var devicePtr: Long = 0L

    // Cached D3D12 resources per shared handle
    private val d3d12TextureCache = HashMap<Long, Long>()
    private val backendRTCache = HashMap<Long, BackendRenderTarget>()

    // Current frame image (created on draw thread using Compose's DirectContext)
    @Volatile private var currentImage: Image? = null
    @Volatile private var imageWidth = 0
    @Volatile private var imageHeight = 0

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

        // Create ANGLE render context FIRST (mpv needs it immediately)
        val angleOk = handle.createAngleRenderContext(1920, 1080)
        if (!angleOk) {
            println("[D3D12Gpu] Failed to create ANGLE render context")
            failed.value = true
            return
        }
        println("[D3D12Gpu] ANGLE render context created")

        // Wait for Compose's D3D12 device
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
            handle.destroyAngleRenderContext()
            failed.value = true
            return
        }

        composeDevice = device
        devicePtr = device.devicePtr
        rawDevicePtr = MPVHandle.extractD3D12DevicePtr(devicePtr)
        if (rawDevicePtr == 0L) {
            println("[D3D12Gpu] Failed to extract raw ID3D12Device from Skiko struct")
            handle.destroyAngleRenderContext()
            failed.value = true
            return
        }

        queuePtr = device.queuePtr.takeIf { it != 0L }
            ?: MPVHandle.createD3D12CommandQueue(devicePtr)
        if (queuePtr == 0L) {
            println("[D3D12Gpu] Failed to get command queue")
            handle.destroyAngleRenderContext()
            failed.value = true
            return
        }

        // Get Compose's DirectContext for use on the draw thread
        composeDirectContext = device.directContext
        println("[D3D12Gpu] Device setup complete: rawDevice=$rawDevicePtr, queue=$queuePtr, context=${composeDirectContext}")

        frameWidth = 1920
        frameHeight = 1080

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

                // Store the shared handle for the draw thread to consume
                val sharedHandle = handle.getAngleSharedTextureHandle()
                if (sharedHandle != 0L) {
                    latestSharedHandle = sharedHandle
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
        backendRTCache.values.forEach { it.close() }
        d3d12TextureCache.values.forEach { MPVHandle.releaseD3D12Resource(it) }
        MPVHandle.releaseD3D12Resource(rawDevicePtr)
        handle.destroyAngleRenderContext()
    }

    private var drawCount = 0L

    /**
     * Called on the Compose draw thread. All Skia operations here use
     * Compose's own DirectContext, so GPU images are directly drawable.
     */
    fun drawTo(canvas: org.jetbrains.skia.Canvas, canvasSize: Size) {
        val sharedHandle = latestSharedHandle
        val w = frameWidth
        val h = frameHeight
        if (sharedHandle == 0L || w <= 0 || h <= 0) {
            if (drawCount % 300 == 0L) println("[D3D12Gpu] drawTo: no frame yet")
            drawCount++
            return
        }

        val directContext = composeDirectContext
        if (directContext == null) {
            if (drawCount % 300 == 0L) println("[D3D12Gpu] drawTo: no DirectContext")
            drawCount++
            return
        }

        // Get or create D3D12 texture for this shared handle
        var d3d12TexturePtr = d3d12TextureCache[sharedHandle]
        if (d3d12TexturePtr == null) {
            d3d12TexturePtr = MPVHandle.openSharedTextureOnD3D12(devicePtr, sharedHandle)
            if (d3d12TexturePtr == 0L) {
                println("[D3D12Gpu] Failed to open shared texture")
                drawCount++
                return
            }
            d3d12TextureCache[sharedHandle] = d3d12TexturePtr
            if (drawCount <= 3) println("[D3D12Gpu] Opened D3D12 texture: handle=$sharedHandle, texture=$d3d12TexturePtr")
        }

        // Get or create BackendRenderTarget
        var rt = backendRTCache[sharedHandle]
        if (rt == null) {
            rt = BackendRenderTarget.makeDirect3D(
                w, h,
                d3d12TexturePtr,
                87, // DXGI_FORMAT_B8G8R8A8_UNORM
                1, 1
            )
            if (rt == null) {
                println("[D3D12Gpu] Failed to create BackendRenderTarget")
                drawCount++
                return
            }
            backendRTCache[sharedHandle] = rt
        }

        // Create Surface from BackendRenderTarget using Compose's DirectContext
        val surface = Surface.makeFromBackendRenderTarget(
            directContext,
            rt,
            SurfaceOrigin.TOP_LEFT,
            SurfaceColorFormat.BGRA_8888,
            ColorSpace.sRGB,
        )

        if (surface != null) {
            // Flush and wait for GPU to sync with D3D11 writes
            directContext.flushAndSubmit(surface, true)

            // Get GPU-backed image snapshot
            val gpuImage = surface.makeImageSnapshot()
            surface.close()

            if (gpuImage != null) {
                val oldImage = currentImage
                currentImage = gpuImage
                imageWidth = w
                imageHeight = h
                oldImage?.close()

                if (drawCount <= 3 || drawCount % 300 == 0L) {
                    println("[D3D12Gpu] drawTo: GPU image ${w}x${h}")
                }
            }
        } else {
            if (drawCount <= 3) println("[D3D12Gpu] makeFromBackendRenderTarget returned null")
        }

        // Draw the current image to canvas
        val image = currentImage ?: run { drawCount++; return }
        if (imageWidth <= 0 || imageHeight <= 0) { drawCount++; return }

        val scale = min(
            canvasSize.width / imageWidth.toFloat(),
            canvasSize.height / imageHeight.toFloat(),
        )
        val scaledW = (imageWidth * scale).roundToInt()
        val scaledH = (imageHeight * scale).roundToInt()
        val offsetX = ((canvasSize.width - scaledW) / 2f).fastCoerceAtLeast(0f)
        val offsetY = ((canvasSize.height - scaledH) / 2f).fastCoerceAtLeast(0f)

        val srcRect = org.jetbrains.skia.Rect.makeWH(imageWidth.toFloat(), imageHeight.toFloat())
        val dstRect = org.jetbrains.skia.Rect.makeXYWH(offsetX, offsetY, scaledW.toFloat(), scaledH.toFloat())

        try {
            canvas.drawImageRect(image, srcRect, dstRect)
        } catch (e: Exception) {
            if (drawCount <= 3) println("[D3D12Gpu] drawTo: Exception: ${e.message}")
        }

        drawCount++
    }

    fun stop() {
        running = false
        renderThread?.let { t ->
            t.interrupt()
            t.join(5000)
        }
        renderThread = null
        currentImage?.close()
        currentImage = null
    }

    private var dumpedComponents = false

    private fun findComposeWindow(): java.awt.Window? {
        val windows = java.awt.Window.getWindows()
        if (windows.isEmpty()) return null
        for (window in windows) {
            val showing = window.isShowing
            val hasSkia = if (showing) containsSkiaLayer(window) else false
            if (showing && !dumpedComponents) {
                dumpedComponents = true
                println("[D3D12Gpu] findComposeWindow: window=${window.javaClass.name}, hasSkiaLayer=$hasSkia")
            }
            if (showing && hasSkia) return window
        }
        return null
    }

    private fun containsSkiaLayer(container: java.awt.Container): Boolean {
        for (component in container.components) {
            val name = component.javaClass.name
            if (name.contains("SkiaLayer") || name.contains("skiko")) return true
            if (component is java.awt.Container && containsSkiaLayer(component)) return true
        }
        return false
    }
}
