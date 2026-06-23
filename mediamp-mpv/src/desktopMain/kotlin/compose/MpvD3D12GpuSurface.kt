/*
 * P3: GPU direct pass-through using Compose's own D3D12 device.
 *
 * Pipeline: mpv ANGLE → D3D11 shared texture → ANGLE sync readback (CPU pixels)
 *           → Skia Bitmap → Compose canvas.drawImage
 *
 * This approach uses ANGLE's reliable D3D11 readback instead of D3D12 texture import,
 * which has synchronization issues causing black frame flickering.
 */

package org.openani.mediamp.mpv.compose

import androidx.compose.foundation.Canvas
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.graphics.drawscope.drawIntoCanvas
import androidx.compose.ui.util.fastCoerceAtLeast
import org.jetbrains.skia.Bitmap
import org.jetbrains.skia.Image
import org.jetbrains.skia.ImageInfo
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

    val failed by renderer.failed
    if (failed) {
        println("[D3D12Gpu] D3D12 path failed, falling back to async readback")
        MpvMediampPlayerSurface(mediampPlayer, modifier)
        return
    }

    var redrawTick by remember { mutableStateOf(0L) }
    LaunchedEffect(renderer) {
        while (true) {
            withFrameNanos { redrawTick++ }
        }
    }

    Canvas(modifier) {
        redrawTick
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

    // Current frame image (CPU-backed via ANGLE readback)
    @Volatile private var currentImage: Image? = null
    @Volatile private var imageWidth = 0
    @Volatile private var imageHeight = 0

    // Double-buffered pixel arrays for render/draw thread handoff
    private var pixelBuffers: Array<ByteArray> = arrayOf(ByteArray(0), ByteArray(0))
    private var writeIdx = 0
    private var readIdx = 1
    @Volatile private var newFrameReady = false

    // Reusable Skia bitmap for creating images
    private val skiaBitmap = Bitmap()

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

        // Create ANGLE render context on render thread
        val angleOk = handle.createAngleRenderContext(1920, 1080)
        if (!angleOk) {
            println("[D3D12Gpu] Failed to create ANGLE render context")
            failed.value = true
            return
        }
        println("[D3D12Gpu] ANGLE render context created")

        var renderedFrames = 0L
        var lastStatTime = System.nanoTime()
        var currentW = 1920
        var currentH = 1080
        var resolutionChecked = false
        val resolutionOut = IntArray(2)
        val sizeOut = IntArray(2)

        while (running) {
            try {
                handle.waitForFrame()
                if (!running) break

                // Render mpv frame via ANGLE (D3D11 shared texture)
                val rendered = handle.renderAngleFrame()
                if (!rendered) continue

                renderedFrames++

                // Read pixels from ANGLE D3D11 texture (sync, reliable)
                val bufferSize = currentW * currentH * 4
                if (pixelBuffers[writeIdx].size < bufferSize) {
                    pixelBuffers[writeIdx] = ByteArray(bufferSize)
                }

                val readOk = handle.copyAnglePixels(pixelBuffers[writeIdx], sizeOut)
                if (readOk) {
                    val w = sizeOut[0]
                    val h = sizeOut[1]
                    if (w > 0 && h > 0) {
                        // Swap buffers
                        val tmp = writeIdx
                        writeIdx = readIdx
                        readIdx = tmp
                        newFrameReady = true
                    }
                }

                // Video resolution check
                if (renderedFrames >= 30 && (!resolutionChecked || renderedFrames % 60 == 0L)) {
                    if (handle.queryVideoResolution(resolutionOut)) {
                        val vw = resolutionOut[0]
                        val vh = resolutionOut[1]
                        if (vw > 0 && vh > 0) {
                            val targetW = minOf(vw, 3840)
                            val targetH = minOf(vh, 2160)
                            if (targetW != currentW || targetH != currentH) {
                                println("[D3D12Gpu] Resizing: ${currentW}x${currentH} -> ${targetW}x${targetH}")
                                handle.resizeAngleRenderContext(targetW, targetH)
                                currentW = targetW
                                currentH = targetH
                                resolutionChecked = true
                            }
                        }
                    }
                }

                // Stats
                val now = System.nanoTime()
                val elapsed = now - lastStatTime
                if (elapsed >= 5_000_000_000L) {
                    val fps = renderedFrames * 1_000_000_000.0 / elapsed
                    println("[D3D12Gpu] FPS: ${"%.1f".format(fps)}, rendered=$renderedFrames, resolution=${currentW}x${currentH}")
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
        handle.destroyAngleRenderContext()
    }

    private var drawCount = 0L

    fun drawTo(canvas: org.jetbrains.skia.Canvas, canvasSize: Size) {
        // Check if a new frame is available from the render thread
        if (newFrameReady) {
            newFrameReady = false

            val pixels = pixelBuffers[readIdx]
            val w = handle.getAngleWidth()
            val h = handle.getAngleHeight()

            if (w > 0 && h > 0 && pixels.size >= w * h * 4) {
                // Create Skia Image from CPU pixels
                val imageInfo = ImageInfo.makeN32Premul(w, h)
                skiaBitmap.allocPixels(imageInfo)
                skiaBitmap.installPixels(imageInfo, pixels, w * 4)
                val newImage = Image.makeFromBitmap(skiaBitmap)

                if (newImage != null) {
                    val oldImage = currentImage
                    currentImage = newImage
                    imageWidth = w
                    imageHeight = h
                    try { oldImage?.close() } catch (_: Exception) {}
                }
            }
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
        try { currentImage?.close() } catch (_: Exception) {}
        currentImage = null
        skiaBitmap.close()
    }
}
