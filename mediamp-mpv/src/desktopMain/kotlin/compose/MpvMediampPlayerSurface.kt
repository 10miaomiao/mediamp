/*
 * Desktop MPV player surface - renders MPV frames
 * ANGLE path: mpv ANGLE → D3D11 shared texture → staging texture readback → Skia Bitmap → Compose Canvas
 * SW fallback: mpv SW → DirectByteBuffer → Skia Bitmap → Compose Canvas
 */

package org.openani.mediamp.mpv.compose

import androidx.compose.foundation.Canvas
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.FilterQuality
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asComposeImageBitmap
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.util.fastCoerceAtLeast
import org.jetbrains.skia.Bitmap
import org.jetbrains.skia.ImageInfo
import org.openani.mediamp.InternalMediampApi
import org.openani.mediamp.mpv.MpvMediampPlayer
import java.nio.ByteBuffer
import javax.swing.SwingUtilities
import kotlin.math.min
import kotlin.math.roundToInt

@OptIn(InternalMediampApi::class)
@Composable
public fun MpvMediampPlayerSurface(
    mediampPlayer: MpvMediampPlayer,
    modifier: Modifier = Modifier,
) {
    val renderState = remember { MpvRenderState(mediampPlayer) }

    DisposableEffect(mediampPlayer) {
        renderState.start()
        onDispose {
            renderState.stop()
        }
    }

    val bitmap by renderState.bitmapState

    Canvas(modifier) {
        val currentBitmap = bitmap ?: return@Canvas
        val imageSize = IntSize(currentBitmap.width, currentBitmap.height)
        val frameSize = Size(size.width, size.height)

        val scale = min(
            frameSize.width / imageSize.width,
            frameSize.height / imageSize.height,
        )
        val scaledW = (imageSize.width * scale).roundToInt()
        val scaledH = (imageSize.height * scale).roundToInt()
        val offsetX = ((frameSize.width - scaledW) / 2f).fastCoerceAtLeast(0f).roundToInt()
        val offsetY = ((frameSize.height - scaledH) / 2f).fastCoerceAtLeast(0f).roundToInt()

        drawImage(
            currentBitmap,
            dstSize = IntSize(scaledW, scaledH),
            dstOffset = IntOffset(offsetX, offsetY),
            filterQuality = FilterQuality.High,
        )
    }
}

@OptIn(InternalMediampApi::class)
private class MpvRenderState(private val player: MpvMediampPlayer) {
    var bitmapState = mutableStateOf<ImageBitmap?>(null)
        private set

    // ANGLE path: staging texture readback buffer
    private val sizeOut = IntArray(2)
    @Volatile
    private var frameBytes = ByteArray(RENDER_WIDTH * RENDER_HEIGHT * 4)

    // SW path: DirectByteBuffer
    private var renderBuffer: Any? = null
    private var renderBufferSize = 0

    // Reusable Skia Bitmap — avoids per-frame Bitmap allocation
    private val skiaBitmap = Bitmap()
    private var lastImageW = 0
    private var lastImageH = 0

    private var renderThread: Thread? = null
    @Volatile
    private var running = false

    // Dynamic resolution tracking
    private var currentRenderW = RENDER_WIDTH
    private var currentRenderH = RENDER_HEIGHT
    private var resolutionChecked = false
    private val resolutionOut = IntArray(2)
    private var successfulRenders = 0

    // Rendering mode
    private var useAngle = false

    fun start() {
        if (running) return
        running = true

        val handle = player.impl

        // ANGLE/SW 渲染上下文将在渲染线程上创建（避免 EGL 线程亲和性问题）
        // 默认尝试 ANGLE，失败则回退 SW
        useAngle = true
        val threadName = "mpv-render"

        renderThread = Thread({
            Thread.currentThread().name = threadName
            var renderedFrames = 0L
            var skippedFrames = 0L
            var lastStatTime = System.nanoTime()
            var lastRenderMs = 0L
            var framesSinceResizeCheck = 0
            var loopCount = 0

            // 在渲染线程上创建渲染上下文（关键：避免 EGL 上下文线程亲和性问题）
            if (useAngle) {
                val angleOk = handle.createAngleRenderContext(RENDER_WIDTH, RENDER_HEIGHT)
                if (angleOk) {
                    println("[Render] ANGLE render context created on render thread (D3D11 shared texture)")
                } else {
                    println("[Render] ANGLE creation failed, falling back to SW")
                    useAngle = false
                }
            }
            if (!useAngle) {
                handle.createSwRenderContext(RENDER_WIDTH, RENDER_HEIGHT)
                renderBufferSize = RENDER_WIDTH * RENDER_HEIGHT * 4
                renderBuffer = handle.createSwRenderBuffer(renderBufferSize)
                println("[Render] SW render context created on render thread (DirectByteBuffer)")
            }

            while (running) {
                try {
                    loopCount++
                    if (loopCount <= 5) println("[Render] Loop #$loopCount: waiting for frame...")
                    handle.waitForFrame()
                    if (loopCount <= 5) println("[Render] Loop #$loopCount: waitForFrame returned")
                    if (!running) break

                    if (loopCount <= 5) println("[Render] Loop #$loopCount: starting render, useAngle=$useAngle")
                    val t0 = System.nanoTime()
                    val rendered = if (useAngle) {
                        renderAngleFrame(handle)
                    } else {
                        renderSwFrame(handle)
                    }
                    if (loopCount <= 5) println("[Render] Loop #$loopCount: rendered=$rendered, useAngle=$useAngle")
                    if (rendered) {
                        lastRenderMs = (System.nanoTime() - t0) / 1_000_000
                        renderedFrames++
                        successfulRenders++

                        val w = sizeOut[0]
                        val h = sizeOut[1]
                        val neededSize = w * h * 4
                        if (neededSize <= 0 || w <= 0 || h <= 0) continue

                        SwingUtilities.invokeLater {
                            // Reuse Skia Bitmap if size unchanged
                            if (w != lastImageW || h != lastImageH) {
                                skiaBitmap.allocPixels(ImageInfo.makeN32Premul(w, h))
                                lastImageW = w
                                lastImageH = h
                            }
                            skiaBitmap.installPixels(ImageInfo.makeN32Premul(w, h), frameBytes, w * 4)
                            bitmapState.value = skiaBitmap.asComposeImageBitmap()
                        }

                        // 在渲染成功后检查视频分辨率（避免在视频加载期间阻塞）
                        // 只在成功渲染 MIN_FRAMES_FOR_RESOLUTION 帧后才查询
                        if (successfulRenders >= MIN_FRAMES_FOR_RESOLUTION && (!resolutionChecked || successfulRenders % 30 == 0)) {
                            if (handle.queryVideoResolution(resolutionOut)) {
                                val vw = resolutionOut[0]
                                val vh = resolutionOut[1]
                                if (vw > 0 && vh > 0) {
                                    val targetW = minOf(vw, 1920)
                                    val targetH = minOf(vh, 1080)
                                    if (targetW != currentRenderW || targetH != currentRenderH) {
                                        println("[Render] Resizing context: ${currentRenderW}x${currentRenderH} -> ${targetW}x${targetH}")
                                        if (useAngle) {
                                            handle.resizeAngleRenderContext(targetW, targetH)
                                        } else {
                                            handle.resizeSwRenderContext(targetW, targetH)
                                            val newBuf = handle.createSwRenderBuffer(targetW * targetH * 4)
                                            val oldBuf = renderBuffer
                                            renderBuffer = newBuf
                                            renderBufferSize = targetW * targetH * 4
                                            frameBytes = ByteArray(renderBufferSize)
                                            if (oldBuf != null) handle.destroySwRenderBuffer(oldBuf)
                                        }
                                        currentRenderW = targetW
                                        currentRenderH = targetH
                                        resolutionChecked = true
                                    }
                                }
                            }
                        }
                    } else {
                        skippedFrames++
                    }

                    val now = System.nanoTime()
                    val elapsed = now - lastStatTime
                    if (elapsed >= 5_000_000_000L) {
                        val totalFrames = renderedFrames + skippedFrames
                        val fps = renderedFrames * 1_000_000_000.0 / elapsed
                        val path = if (useAngle) "ANGLE" else "SW"
                        println("[Render] $path FPS: ${"%.1f".format(fps)}, rendered=$renderedFrames, skipped=$skippedFrames, total=$totalFrames, render=${currentRenderW}x${currentRenderH}, last render=${lastRenderMs}ms")
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
        }, threadName).apply {
            isDaemon = true
            start()
        }
    }

    /**
     * ANGLE path: render ANGLE frame, then read pixels via D3D11 staging texture.
     * Returns true if a frame was rendered successfully.
     */
    private fun renderAngleFrame(handle: org.openani.mediamp.mpv.MPVHandle): Boolean {
        if (!handle.renderAngleFrame()) return false

        val w = currentRenderW
        val h = currentRenderH
        val neededSize = w * h * 4
        if (frameBytes.size < neededSize) {
            frameBytes = ByteArray(neededSize)
        }

        // Read pixels from D3D11 shared texture via staging texture (fast, ~5ms for 1080p)
        if (!handle.readPixelsFromSharedTexture(frameBytes, sizeOut)) return false
        return true
    }

    /**
     * SW path: render SW frame to DirectByteBuffer, then copy to ByteArray.
     * Returns true if a frame was rendered successfully.
     */
    private fun renderSwFrame(handle: org.openani.mediamp.mpv.MPVHandle): Boolean {
        val buf = renderBuffer ?: return false
        if (!handle.renderSwFrameToBuffer(buf, renderBufferSize, sizeOut)) return false

        val w = sizeOut[0]
        val h = sizeOut[1]
        val neededSize = w * h * 4
        if (neededSize <= 0 || w <= 0 || h <= 0) return false

        if (frameBytes.size < neededSize) {
            frameBytes = ByteArray(neededSize)
        }

        val byteBuf = buf as ByteBuffer
        byteBuf.rewind()
        byteBuf.limit(neededSize)
        byteBuf.get(frameBytes, 0, neededSize)
        return true
    }

    fun stop() {
        running = false
        renderThread?.interrupt()
        renderThread = null
        // Free SW DirectByteBuffer if used
        if (!useAngle) {
            val buf = renderBuffer
            if (buf != null) {
                player.impl.destroySwRenderBuffer(buf)
                renderBuffer = null
            }
        }
    }

    companion object {
        private const val RENDER_WIDTH = 1920
        private const val RENDER_HEIGHT = 1080
        // 视频加载期间 mpv_get_property("video-params") 可能阻塞，跳过前 N 帧
        private const val MIN_FRAMES_FOR_RESOLUTION = 60
    }
}
