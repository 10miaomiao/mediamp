/*
 * Desktop MPV player surface - renders offscreen MPV frames to Compose Canvas
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
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt

/**
 * Composable that renders MPV video frames on desktop.
 *
 * Uses offscreen OpenGL rendering via mpv_render_context, reads pixels back,
 * and draws them to a Compose Canvas using Skia Bitmap.
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
    private val skiaBitmap = Bitmap()
    var bitmapState = mutableStateOf<ImageBitmap?>(null)
        private set

    private var renderThread: Thread? = null
    @Volatile
    private var running = false

    fun start() {
        if (running) return
        running = true

        // Create render context with initial size
        val handle = player.impl
        handle.createRenderContext(RENDER_WIDTH, RENDER_HEIGHT)

        renderThread = Thread({
            Thread.currentThread().name = "mpv-render"
            while (running) {
                try {
                    if (handle.renderFrame()) {
                        val pixels = handle.getRenderPixels()
                        val w = handle.getRenderWidth()
                        val h = handle.getRenderHeight()

                        if (pixels != null && w > 0 && h > 0) {
                            val imageInfo = ImageInfo.makeN32Premul(w, h)
                            skiaBitmap.allocPixels(imageInfo)
                            skiaBitmap.installPixels(
                                imageInfo,
                                pixels,
                                w * 4,
                            )
                            bitmapState.value = skiaBitmap.asComposeImageBitmap()
                        }
                    }

                    // ~60fps render loop
                    Thread.sleep(16)
                } catch (e: InterruptedException) {
                    break
                } catch (e: Exception) {
                    e.printStackTrace()
                    Thread.sleep(100)
                }
            }
        }, "mpv-render").apply {
            isDaemon = true
            start()
        }
    }

    fun stop() {
        running = false
        renderThread?.interrupt()
        renderThread = null
        player.impl.destroyRenderContext()
    }

    companion object {
        private const val RENDER_WIDTH = 1920
        private const val RENDER_HEIGHT = 1080
    }
}
