/*
 * Copyright (C) 2024 OpenAni and contributors.
 *
 * Use of this source code is governed by the GNU GENERAL PUBLIC LICENSE version 3 license, which can be found at the following link.
 *
 * https://github.com/open-ani/mediamp/blob/main/LICENSE
 */

package org.openani.mediamp.mpv

import kotlinx.coroutines.flow.MutableStateFlow
import org.openani.mediamp.AbstractMediampPlayer
import org.openani.mediamp.InternalMediampApi
import org.openani.mediamp.PlaybackState
import org.openani.mediamp.features.PlayerFeatures
import org.openani.mediamp.features.buildPlayerFeatures
import org.openani.mediamp.internal.Platform
import org.openani.mediamp.internal.currentPlatform
import org.openani.mediamp.metadata.MediaProperties
import org.openani.mediamp.source.MediaData
import org.openani.mediamp.source.SeekableInputMediaData
import org.openani.mediamp.source.UriMediaData
import kotlin.coroutines.CoroutineContext

@kotlin.OptIn(InternalMediampApi::class)
actual class MpvMediampPlayer (
    context: Any,
    parentCoroutineContext: CoroutineContext,
) : AbstractMediampPlayer<MpvMediampPlayer.MPVPlayerData>(parentCoroutineContext) {
    class MPVPlayerData(mediaData: MediaData) : Data(mediaData)

    private val handle = MPVHandle(context)
    
    private val eventListener = object : EventListener {
        override fun onPropertyChange(name: String) {
        }

        override fun onPropertyChange(name: String, value: Boolean) {
            when (name) {
                "pause" -> playbackState.value =
                    if (value) PlaybackState.PAUSED else PlaybackState.PLAYING
                "paused-for-cache" -> playbackState.value =
                    if (value) PlaybackState.PAUSED_BUFFERING else PlaybackState.PLAYING

            }
        }

        override fun onPropertyChange(name: String, value: Long) {
            when (name) {
                "time-pos/full" -> currentPositionMillis.value = value * 1000
                "duration/full" -> mediaProperties.value =
                    if (mediaProperties.value == null) MediaProperties(null, value * 1000)
                    else mediaProperties.value?.copy(durationMillis = value * 1000)
            }
        }

        override fun onPropertyChange(name: String, value: Double) {
        }

        override fun onPropertyChange(name: String, value: String) {
            when (name) {
                "media-title" -> mediaProperties.value =
                    if (mediaProperties.value == null) MediaProperties(value, -1)
                    else mediaProperties.value?.copy(title = value)
            }
        }

    }

    override val impl: MPVHandle get() = handle

    override val currentPositionMillis: MutableStateFlow<Long> = MutableStateFlow(0L)
    
    override val mediaProperties: MutableStateFlow<MediaProperties?> = MutableStateFlow(null)

    override val features: PlayerFeatures = buildPlayerFeatures { }
    
    override fun getCurrentMediaProperties(): MediaProperties? {
        return mediaProperties.value
    }

    override fun getCurrentPlaybackState(): PlaybackState {
        return playbackState.value
    }

    override fun getCurrentPositionMillis(): Long {
        return currentPositionMillis.value
    }

    init {
        handle.setEventListener(eventListener)

        handle.option("config", "no")

        // 使用 vo=libmpv，由 render context (ANGLE/SW) 驱动渲染
        handle.option("vo", "libmpv")

        when (currentPlatform()) {
            is Platform.Android -> {
                handle.option("gpu-context", "android")
                handle.option("opengl-es", "yes")
                handle.option("ao", "audiotrack,opensles")
            }
            is Platform.Windows -> {
                handle.option("ao", "wasapi")
            }
            is Platform.MacOS -> {
                handle.option("ao", "coreaudio")
            }
            else -> { }
        }

        handle.option("hwdec", "auto")
        handle.option("hwdec-codecs", "h264,hevc,mpeg4,mpeg2video,vp8,vp9,av1")
        handle.option("sw-fast", "yes")
        handle.option("input-default-bindings", "yes")

        // Limit demuxer cache
        val cacheMegs = if (limitDemuxer()) 32 else 64
        handle.option("demuxer-max-bytes", "${cacheMegs * 1024 * 1024}")
        handle.option("demuxer-max-back-bytes", "${cacheMegs * 1024 * 1024}")

        handle.option("save-position-on-quit", "no")
        handle.option("idle", "yes")
        handle.option("keep-open", "always")

        handle.initialize()

        // 创建渲染上下文（在 mpv_initialize 之后，loadfile 之前）
        if (currentPlatform() !is Platform.Android) {
            // 桌面端优先使用 ANGLE 渲染上下文（D3D11 GPU 加速 + staging texture 像素回读）
            // 回退到 SW 渲染上下文（CPU 渲染 + DirectByteBuffer）
            val angleCreated = handle.createAngleRenderContext(1920, 1080)
            if (angleCreated) {
                println("[MPV] ANGLE render context created successfully")
            } else {
                println("[MPV] ANGLE not available, falling back to SW render context")
                handle.createSwRenderContext(1920, 1080)
            }
        }

        handle.observeProperty("time-pos/full", MPVFormat.MPV_FORMAT_INT64)
        handle.observeProperty("duration/full", MPVFormat.MPV_FORMAT_INT64)
        handle.observeProperty("pause", MPVFormat.MPV_FORMAT_FLAG)
        handle.observeProperty("paused-for-cache", MPVFormat.MPV_FORMAT_FLAG)
        handle.observeProperty("speed", MPVFormat.MPV_FORMAT_STRING) // todo
        
        handle.observeProperty("media-title", MPVFormat.MPV_FORMAT_STRING) // to
        handle.observeProperty("metadata", MPVFormat.MPV_FORMAT_NONE)
        handle.observeProperty("hwdec-current", MPVFormat.MPV_FORMAT_NONE)
        // video-params observation removed: resolution is now polled in render loop
    }
    
    @InternalMediampApi
    fun attachRenderSurface(surface: Any): Boolean {
        return attachSurface(handle.ptr, surface)
    }

    @InternalMediampApi
    fun detachRenderSurface(): Boolean {
        return detachSurface(handle.ptr)
    }

    /**
     * 创建与 mpv 共享的 GL 上下文（GPU 纹理直出路径）。
     * 传入 Skia 的 HDC 和 mpv 的 HGLRC，创建一个在 Skia HDC 上下文的新 HGLRC，
     * 该 HGLRC 与 mpv 的 HGLRC 共享 GL 对象（纹理、FBO 等）。
     *
     * @param hdc Skia 的 HDC（从 WindowsOpenGLRedrawer.device 反射获取）
     * @param mpvHglrc mpv 的 HGLRC（从 createGlSharedContext 获取）
     */
    @InternalMediampApi
    fun setupGlSharedContext(mpvHglrc: Long): Boolean {
        return MPVHandle.setupGlContextWithHandles(handle.ptr, mpvHglrc)
    }

    override suspend fun setMediaDataImpl(data: MediaData): MPVPlayerData {
        println("[MPV] setMediaDataImpl called, data type=${data::class}, data=$data")
        return when (data) {
            is UriMediaData -> {
                val headers = data.headers.toMutableMap()
                println("[MPV] setMediaDataImpl: URI=${data.uri}")

                // 清除播放列表和旧的外部音频轨道
                handle.command("stop")
                handle.command("playlist-clear")

                // 设置 User-Agent（仅当 headers 中有指定时）
                val ua = headers.remove("User-Agent") ?: headers.remove("user-agent")
                if (ua != null) {
                    handle.setPropertyString("user-agent", ua)
                }

                // 合并所有 HTTP 请求头为字符串列表，一次性设置
                if (headers.isNotEmpty()) {
                    val headerArray = headers.entries.map { "${it.key}: ${it.value}" }.toTypedArray()
                    handle.setPropertyStringList("http-header-fields", headerArray)
                    println("[MPV] setMediaDataImpl: http-header-fields=${headerArray.joinToString(",")}")
                }

                // 立即加载并播放，确保 HTTP 头在流打开前已生效
                handle.setPropertyBoolean("pause", false)
                val result = handle.command("loadfile", data.uri)
                println("[MPV] setMediaDataImpl: loadfile result=$result")

                MPVPlayerData(data)
            }
            is SeekableInputMediaData -> {
                TODO()
            }
            else -> {
                println("[MPV] setMediaDataImpl: unsupported data type: ${data::class}")
                throw IllegalArgumentException("Unsupported media data type: ${data::class}")
            }
        }
    }

    override fun resumeImpl() {
        val state = playbackState.value
        println("[MPV] resumeImpl called, state=$state")
        when (state) {
            PlaybackState.READY -> {
                val media = openResource.value
                if (media == null) {
                    println("[MPV] resumeImpl: openResource is null!")
                    return
                }
                when (val data = media.mediaData) {
                    is UriMediaData -> {
                        // loadfile 已在 setMediaDataImpl 中调用，此处只需更新状态
                        playbackState.value = PlaybackState.PLAYING
                    }
                    is SeekableInputMediaData -> TODO()
                    else -> {
                        println("[MPV] resumeImpl: unsupported media type: ${media.mediaData::class}")
                    }
                }
            }
            PlaybackState.PAUSED -> {
                println("[MPV] resumeImpl: unpausing")
                handle.command("cycle", "pause")
            }
            PlaybackState.PLAYING -> {
                println("[MPV] resumeImpl: toggling pause")
                handle.command("cycle", "pause")
            }
            else -> {
                println("[MPV] resumeImpl: unhandled state=$state")
            }
        }
    }

    override fun pauseImpl() {
        if (playbackState.value == PlaybackState.PAUSED) return
        handle.command("cycle", "pause")
    }

    override fun seekTo(positionMillis: Long) {
        handle.command("seek", positionMillis.toString(), "absolute+exact")
        currentPositionMillis.value = positionMillis
    }

    override fun skip(deltaMillis: Long) {
        handle.command("seek", deltaMillis.toString(), "relative+relative")
        currentPositionMillis.value += deltaMillis
    }

    override fun stopPlaybackImpl() {
        handle.command("stop")
        currentPositionMillis.value = 0L
        playbackState.value = PlaybackState.FINISHED
    }
    

    override fun closeImpl() {
        // Stop playback first (may be no-op if already stopped)
        try { handle.command("stop") } catch (_: Exception) {}
        handle.destroyAngleRenderContext()
        handle.destroyGlRenderContext()
        handle.destroySwRenderContext()
        handle.destroy()
        handle.close()
    }
    
    companion object
}
