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

    private val skiaBitmap = Bitmap()
    @Volatile
    private var pixelBuffer = ByteArray(RENDER_WIDTH * RENDER_HEIGHT * 4)
    private val sizeOut = IntArray(2)

    private var renderThread: Thread? = null
    @Volatile
    private var running = false

    private var bitmapW = 0
    private var bitmapH = 0

    fun start() {
        if (running) return
        running = true

        val handle = player.impl

        renderThread = Thread({
            Thread.currentThread().name = "mpv-sw-render"
            var frameCount = 0
            var lastFpsTime = System.nanoTime()
            while (running) {
                try {
                    handle.waitForFrame()
                    if (!running) break

                    val t0 = System.nanoTime()
                    if (handle.renderSwFrame()) {
                        val renderMs = (System.nanoTime() - t0) / 1_000_000

                        val neededSize = handle.getSwWidth() * handle.getSwHeight() * 4
                        if (neededSize <= 0) continue
                        if (pixelBuffer.size < neededSize) {
                            pixelBuffer = ByteArray(neededSize)
                        }
                        if (handle.copySwPixels(pixelBuffer, sizeOut)) {
                            val w = sizeOut[0]
                            val h = sizeOut[1]
                            if (w > 0 && h > 0) {
                                frameCount++
                                val now = System.nanoTime()
                                val elapsed = now - lastFpsTime
                                if (elapsed >= 5_000_000_000L) { // every 5 seconds
                                    val fps = frameCount * 1_000_000_000.0 / elapsed
                                    println("[Render] SW FPS: ${"%.1f".format(fps)}, frames=$frameCount, last render=${renderMs}ms")
                                    frameCount = 0
                                    lastFpsTime = now
                                }
                                val buf = pixelBuffer
                                SwingUtilities.invokeLater {
                                    if (w != bitmapW || h != bitmapH) {
                                        bitmapW = w
                                        bitmapH = h
                                        skiaBitmap.allocPixels(ImageInfo.makeN32Premul(w, h))
                                    }
                                    skiaBitmap.installPixels(ImageInfo.makeN32Premul(w, h), buf, w * 4)
                                    bitmapState.value = skiaBitmap.asComposeImageBitmap()
                                }
                            }
                        }
                    }
                } catch (e: InterruptedException) {
                    break
                } catch (e: Exception) {
                    e.printStackTrace()
                    Thread.sleep(100)
                }
            }
        }, "mpv-sw-render").apply {
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
