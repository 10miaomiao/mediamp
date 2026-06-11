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
import org.jetbrains.skia.ColorAlphaType
import org.jetbrains.skia.ColorType
import org.jetbrains.skia.ImageInfo
import org.openani.mediamp.InternalMediampApi
import org.openani.mediamp.mpv.MpvMediampPlayer
import javax.swing.SwingUtilities
import kotlin.math.min
import kotlin.math.roundToInt

/**
 * Composable that renders MPV video frames on desktop.
 *
 * Uses vo=libmpv with software render context (MPV_RENDER_PARAM_SW_*).
 * mpv writes pixels directly to a CPU buffer, no OpenGL needed.
 */
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

    private var renderThread: Thread? = null
    @Volatile
    private var running = false

    fun start() {
        if (running) return
        running = true

        val handle = player.impl

        renderThread = Thread({
            Thread.currentThread().name = "mpv-sw-render"
            while (running) {
                try {
                    // Block until mpv signals a new frame is ready (event-driven, no polling)
                    handle.waitForFrame()
                    if (!running) break

                    if (handle.renderSwFrame()) {
                        val w = handle.getVideoWidth()
                        val h = handle.getVideoHeight()

                        if (w > 0 && h > 0) {
                            val neededSize = w * h * 4
                            if (pixelBuffer.size < neededSize) {
                                pixelBuffer = ByteArray(neededSize)
                            }
                            if (handle.copySwPixels(pixelBuffer)) {
                                val buf = pixelBuffer
                                SwingUtilities.invokeLater {
                                    val imageInfo = ImageInfo.makeN32Premul(w, h)
                                    skiaBitmap.allocPixels(imageInfo)
                                    skiaBitmap.installPixels(imageInfo, buf, w * 4)
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
        // SW render context is destroyed in mpv_handle_t::destroy()
    }

    companion object {
        private const val RENDER_WIDTH = 1920
        private const val RENDER_HEIGHT = 1080
    }
}
