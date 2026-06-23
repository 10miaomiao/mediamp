/*
 * Desktop MPV GPU direct output surface.
 *
 * Pipeline: mpv GL render → FBO → BackendRenderTarget → Surface.makeFromBackendRenderTarget
 *           → makeImageSnapshot → native Canvas.drawImage
 *
 * Zero CPU readback. All operations are GPU-side.
 * Uses WGL shared context: mpv's GL context shares texture objects with Skia's GL context.
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
import org.jetbrains.skia.ColorAlphaType
import org.jetbrains.skia.ColorSpace
import org.jetbrains.skia.ColorType
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

/**
 * GPU direct output surface for MPV video rendering.
 *
 * Requires the MPV player to have a GL render context and a shared GL context
 * with Skia's GL context (setup via [MpvMediampPlayer.setupGlSharedContext]).
 */
@OptIn(InternalMediampApi::class)
@Composable
public fun MpvGpuPlayerSurface(
    mediampPlayer: MpvMediampPlayer,
    modifier: Modifier = Modifier,
) {
    val renderer = remember { GpuTextureRenderer(mediampPlayer) }

    DisposableEffect(mediampPlayer) {
        renderer.start()
        onDispose { renderer.stop() }
    }

    // Trigger recomposition when a new frame is ready
    val frameTrigger by renderer.frameTrigger

    Canvas(modifier) {
        frameTrigger // read to establish subscription
        drawIntoCanvas { canvas ->
            renderer.drawTo(canvas.nativeCanvas, Size(size.width, size.height))
        }
    }
}

/**
 * Manages the GPU texture direct output pipeline.
 *
 * Thread model:
 * - mpv-gpu-render thread: renders mpv frames to FBO, creates snapshot image
 * - Skia render thread (EDT): draws image to canvas
 *
 * GL context sharing:
 * - mpv uses a shared HGLRC created via wglCreateContextAttribsARB with Skia's HGLRC
 * - FBO and textures created by mpv are visible to Skia via shared GL namespace
 */
@OptIn(InternalMediampApi::class)
private class GpuTextureRenderer(private val player: MpvMediampPlayer) {
    private val handle: MPVHandle = player.impl as MPVHandle

    // Shared GL DirectContext for Skia operations
    private var directContext: DirectContext? = null

    // Current frame state (written by render thread, read by draw thread)
    @Volatile private var currentImage: Image? = null
    @Volatile private var imageWidth = 0
    @Volatile private var imageHeight = 0

    // Frame trigger for recomposition
    val frameTrigger = mutableStateOf(0L)

    private var renderThread: Thread? = null
    @Volatile private var running = false

    private val logFile = java.io.File(System.getProperty("java.io.tmpdir"), "gpu_render.log")

    private fun log(msg: String) {
        try { logFile.appendText("[${System.currentTimeMillis()}] $msg\n") } catch (_: Exception) {}
    }

    fun start() {
        if (running) return
        running = true
        log("[GpuRender] start() called")

        renderThread = Thread({
            renderLoop()
        }, "mpv-gpu-render").apply {
            isDaemon = true
            start()
        }
    }

    private fun renderLoop() {
        log("[GpuRender] renderLoop started")
        var renderedFrames = 0L
        var skippedFrames = 0L
        var lastStatTime = System.nanoTime()

        while (running) {
            try {
                // Initialize shared GL context on first iteration
                if (directContext == null) {
                    // Create mpv's GL render context first (required for setupContext)
                    val glOk = handle.createGlRenderContext(1920, 1080)
                    if (!glOk) {
                        log("[GpuRender] Failed to create GL render context")
                        Thread.sleep(1000)
                        continue
                    }
                    log("[GpuRender] GL render context created")

                    setupContext()
                    if (directContext == null) {
                        Thread.sleep(1000)
                        continue
                    }
                }

                // Poll for frame readiness instead of blocking (prevents deadlock on close)
                while (running && !handle.hasPendingFrame()) {
                    Thread.sleep(5)
                }
                if (!running) break

                // Render mpv frame to FBO (this switches to mpv's GL context)
                if (!handle.renderGlFrame()) {
                    skippedFrames++
                    continue
                }

                // Ensure mpv's rendering is complete on GPU
                handle.finishGlRender()

                // Switch to the shared GL context for Skia operations
                if (!MPVHandle.makeContextCurrent()) {
                    log("[GpuRender] Failed to make shared context current")
                    skippedFrames++
                    continue
                }

                val ctx = directContext
                if (ctx == null) {
                    log("[GpuRender] directContext is null")
                    skippedFrames++
                    continue
                }

                val fboId = handle.getGlFboId()
                val w = handle.getGlWidth()
                val h = handle.getGlHeight()
                if (fboId == 0 || w <= 0 || h <= 0) {
                    log("[GpuRender] invalid FBO or size: fbo=$fboId, ${w}x${h}")
                    skippedFrames++
                    continue
                }

                // Close old Image
                currentImage?.close()
                currentImage = null

                try {
                    // Reset Skia's cached GL state after context switching
                    ctx.resetAll()

                    // Create BackendRenderTarget from mpv's FBO
                    // mpv uses rgba16f format for its FBO
                    val rt = BackendRenderTarget.makeGL(
                        w, h,
                        0,          // sampleCnt (no MSAA)
                        8,          // stencilBits
                        fboId,      // mpv's FBO ID
                        GL_RGBA16F, // fbFormat (0x881A) - mpv uses rgba16f
                    )

                    // Create a Skia Surface wrapping the FBO
                    val surface = Surface.makeFromBackendRenderTarget(
                        ctx,
                        rt,
                        SurfaceOrigin.TOP_LEFT,
                        SurfaceColorFormat.RGBA_F16,
                        ColorSpace.sRGB,
                    )

                    if (surface != null) {
                        // Get a snapshot image from the surface.
                        // makeImageSnapshot() copies the pixel data, so it's safe
                        // even when mpv overwrites the FBO in the next frame.
                        val image = surface.makeImageSnapshot()
                        surface.close()

                        currentImage = image
                        imageWidth = w
                        imageHeight = h

                        // Trigger recomposition on EDT
                        SwingUtilities.invokeLater {
                            frameTrigger.value = System.nanoTime()
                        }
                        renderedFrames++
                    } else {
                        log("[GpuRender] makeFromBackendRenderTarget returned null")
                        skippedFrames++
                    }
                } catch (e: Exception) {
                    log("[GpuRender] Exception in image creation: ${e.message}")
                    if (renderedFrames == 0L) e.printStackTrace()
                }

                // Print stats every 5 seconds
                val now = System.nanoTime()
                val elapsed = now - lastStatTime
                if (elapsed >= 5_000_000_000L) {
                    val fps = renderedFrames * 1_000_000_000.0 / elapsed
                    log("[GpuRender] FPS: ${"%.1f".format(fps)}, rendered=$renderedFrames, skipped=$skippedFrames, resolution=${w}x${h}")
                    renderedFrames = 0
                    skippedFrames = 0
                    lastStatTime = now
                }
            } catch (e: InterruptedException) {
                break
            } catch (e: Exception) {
                e.printStackTrace()
                Thread.sleep(100)
            }
        }
    }

    /**
     * Initialize the shared GL context and create a DirectContext for Skia.
     */
    private fun setupContext() {
        log("[GpuRender] setupContext() called")

        // Get mpv's HGLRC (created during createGlRenderContext)
        val mpvHglrc = handle.createGlSharedContext()
        if (mpvHglrc == 0L) {
            log("[GpuRender] Failed to get mpv's HGLRC (createGlRenderContext not called?)")
            return
        }

        log("[GpuRender] Setting up shared GL context: mpvHglrc=$mpvHglrc")

        // Create shared context on mpv's own HDC (passed via handle.ptr)
        if (!player.setupGlSharedContext(mpvHglrc)) {
            log("[GpuRender] Failed to create shared GL context")
            return
        }

        // Make the shared context current on this thread
        if (!MPVHandle.makeContextCurrent()) {
            log("[GpuRender] Failed to make shared context current")
            return
        }

        // Create a DirectContext from the shared GL context
        val ctx = DirectContext.makeGL()
        if (ctx == null) {
            log("[GpuRender] Failed to create DirectContext.makeGL()")
            return
        }

        directContext = ctx
        log("[GpuRender] Shared GL context and DirectContext initialized successfully")
    }

    /**
     * Draw the current video frame to the Skia Canvas.
     * Called on the Skia render thread during Compose draw pass.
     */
    fun drawTo(canvas: org.jetbrains.skia.Canvas, canvasSize: Size) {
        val image = currentImage ?: return
        val w = imageWidth
        val h = imageHeight
        if (w <= 0 || h <= 0) return

        // Calculate aspect-ratio-preserving scale
        val scale = min(
            canvasSize.width / w.toFloat(),
            canvasSize.height / h.toFloat(),
        )
        val scaledW = (w * scale).roundToInt()
        val scaledH = (h * scale).roundToInt()
        val offsetX = ((canvasSize.width - scaledW) / 2f).fastCoerceAtLeast(0f)
        val offsetY = ((canvasSize.height - scaledH) / 2f).fastCoerceAtLeast(0f)

        // Draw directly to native Skia canvas (no bitmap conversion, no CPU readback)
        canvas.drawImageRect(
            image,
            org.jetbrains.skia.Rect.makeWH(w.toFloat(), h.toFloat()),
            org.jetbrains.skia.Rect.makeXYWH(offsetX, offsetY, scaledW.toFloat(), scaledH.toFloat()),
        )
    }

    @Volatile private var stopped = false

    fun stop() {
        if (stopped) return
        stopped = true

        // 1. Close player first (destroys GL context, wakes up render thread)
        try { player.close() } catch (_: Exception) {}

        // 2. Signal render thread to exit
        running = false
        renderThread?.let { t ->
            t.interrupt()
            t.join(5000)
        }
        renderThread = null

        // 3. Clean up GL resources
        currentImage?.close()
        currentImage = null
        directContext?.close()
        directContext = null
    }

    companion object {
        // GL constants
        private const val GL_RGBA16F = 0x881A
    }
}
