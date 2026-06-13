/*
 * Desktop MPV player surface - renders MPV frames via software render context
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

    @Volatile
    private var pixelBuffer = ByteArray(RENDER_WIDTH * RENDER_HEIGHT * 4)
    private val sizeOut = IntArray(2)

    private var renderThread: Thread? = null
    @Volatile
    private var running = false

    // Dynamic resolution tracking
    private var currentRenderW = RENDER_WIDTH
    private var currentRenderH = RENDER_HEIGHT
    private var resolutionChecked = false
    private val resolutionOut = IntArray(2)

    // Rendering mode
    private var useAngle = false

    fun start() {
        if (running) return
        running = true

        val handle = player.impl

        // Detect which render context is available
        useAngle = false // ANGLE D3D11 disabled for now (performance/stability issues)
        val threadName = "mpv-sw-render"
        println("[Render] Using SW render path")

        renderThread = Thread({
            Thread.currentThread().name = threadName
            var renderedFrames = 0L
            var skippedFrames = 0L
            var lastStatTime = System.nanoTime()
            var lastRenderMs = 0L
            var framesSinceResizeCheck = 0
            while (running) {
                try {
                    handle.waitForFrame()
                    if (!running) break

                    // Periodically check video resolution and resize if needed
                    framesSinceResizeCheck++
                    if (!resolutionChecked || framesSinceResizeCheck >= 30) {
                        framesSinceResizeCheck = 0
                        if (handle.queryVideoResolution(resolutionOut)) {
                            val vw = resolutionOut[0]
                            val vh = resolutionOut[1]
                            if (vw > 0 && vh > 0) {
                                val targetW = minOf(vw, 1920)
                                val targetH = minOf(vh, 1080)
                                if (targetW != currentRenderW || targetH != currentRenderH) {
                                    println("[Render] Resizing ${if (useAngle) "ANGLE" else "SW"} context: ${currentRenderW}x${currentRenderH} -> ${targetW}x${targetH}")
                                    if (useAngle) {
                                        handle.resizeAngleRenderContext(targetW, targetH)
                                    } else {
                                        handle.resizeSwRenderContext(targetW, targetH)
                                    }
                                    currentRenderW = targetW
                                    currentRenderH = targetH
                                }
                                resolutionChecked = true
                            }
                        }
                    }

                    val t0 = System.nanoTime()
                    val rendered = if (useAngle) {
                        handle.renderAngleFrame()
                    } else {
                        handle.renderSwFrame()
                    }

                    if (rendered) {
                        lastRenderMs = (System.nanoTime() - t0) / 1_000_000
                        renderedFrames++

                        val w: Int
                        val h: Int
                        val copied: Boolean

                        if (useAngle) {
                            val neededSize = handle.getAngleWidth() * handle.getAngleHeight() * 4
                            if (neededSize <= 0) continue
                            if (pixelBuffer.size < neededSize) {
                                pixelBuffer = ByteArray(neededSize)
                            }
                            copied = handle.copyAnglePixels(pixelBuffer, sizeOut)
                            w = sizeOut[0]
                            h = sizeOut[1]
                        } else {
                            val neededSize = handle.getSwWidth() * handle.getSwHeight() * 4
                            if (neededSize <= 0) continue
                            if (pixelBuffer.size < neededSize) {
                                pixelBuffer = ByteArray(neededSize)
                            }
                            copied = handle.copySwPixels(pixelBuffer, sizeOut)
                            w = sizeOut[0]
                            h = sizeOut[1]
                        }

                        if (copied && w > 0 && h > 0) {
                            val buf = pixelBuffer
                            SwingUtilities.invokeLater {
                                val bmp = Bitmap()
                                bmp.allocPixels(ImageInfo.makeN32Premul(w, h))
                                bmp.installPixels(ImageInfo.makeN32Premul(w, h), buf, w * 4)
                                bitmapState.value = bmp.asComposeImageBitmap()
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
                        val mode = if (useAngle) "ANGLE" else "SW"
                        println("[Render] $mode FPS: ${"%.1f".format(fps)}, rendered=$renderedFrames, skipped=$skippedFrames, total=$totalFrames, render=${currentRenderW}x${currentRenderH}, last render=${lastRenderMs}ms")
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

    fun stop() {
        running = false
        renderThread?.interrupt()
        renderThread = null
    }

    companion object {
        private const val RENDER_WIDTH = 1920
        private const val RENDER_HEIGHT = 1080
    }
}
