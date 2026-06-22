/*
 * Copyright (C) 2024-2025 OpenAni and contributors.
 *
 * Use of this source code is governed by the Apache License version 2 license, which can be found at the following link.
 *
 * https://github.com/open-ani/mediamp/blob/main/LICENSE
 */

package org.openani.mediamp.mpv

@OptIn(ExperimentalStdlibApi::class)
class MPVHandle private constructor(internal val ptr: Long) : AutoCloseable {
    // private val cleanable = cleaner.register(this, ReferenceHolder(ptr))
    private var eventListener: EventListener? = null

    constructor(context: Any) : this(createHandle(context)) {
        if (ptr == 0L) throw IllegalStateException("Failed to create native mpv handle")
    }

    fun initialize(): Boolean {
        return nInitialize(ptr)
    }

    fun setEventListener(listener: EventListener) {
        eventListener = listener
        nSetEventListener(ptr, listener)
    }

    fun command(vararg command: String): Boolean {
        return nCommand(ptr, command)
    }

    fun option(key: String, value: String): Boolean {
        return nOption(ptr, key, value)
    }

    fun getPropertyInt(name: String): Int {
        return nGetPropertyInt(ptr, name)
    }

    fun getPropertyBoolean(name: String): Boolean {
        return nGetPropertyBoolean(ptr, name)
    }

    fun getPropertyDouble(name: String): Double {
        return nGetPropertyDouble(ptr, name)
    }

    fun getPropertyString(name: String): String {
        return nGetPropertyString(ptr, name)
    }

    fun setPropertyInt(name: String, value: Int): Boolean {
        return nSetPropertyInt(ptr, name, value)
    }

    fun setPropertyLong(name: String, value: Long): Boolean {
        return nSetPropertyLong(ptr, name, value)
    }

    fun setPropertyBoolean(name: String, value: Boolean): Boolean {
        return nSetPropertyBoolean(ptr, name, value)
    }

    fun setPropertyStringList(name: String, values: Array<String>): Boolean {
        return nSetPropertyStringList(ptr, name, values)
    }

    fun setPropertyDouble(name: String, value: Double): Boolean {
        return nSetPropertyDouble(ptr, name, value)
    }

    fun setPropertyString(name: String, value: String): Boolean {
        return nSetPropertyString(ptr, name, value)
    }

    fun observeProperty(name: String, format: MPVFormat, replyData: Long = 0L): Boolean {
        return nObserveProperty(ptr, name, format.ordinal, replyData)
    }

    fun unobserveProperty(replyData: Long): Boolean {
        return nUnobserveProperty(ptr, replyData)
    }

    // Software render context (vo=libmpv)

    fun createSwRenderContext(width: Int, height: Int): Boolean {
        return nCreateSwRenderContext(ptr, width, height)
    }

    fun resizeSwRenderContext(width: Int, height: Int): Boolean {
        return nResizeSwRenderContext(ptr, width, height)
    }

    fun renderSwFrame(): Boolean {
        return nRenderSwFrame(ptr)
    }

    /**
     * Allocate a native DirectByteBuffer for SW rendering.
     * Returns the DirectByteBuffer object (JVM: java.nio.ByteBuffer).
     */
    fun createSwRenderBuffer(size: Int): Any {
        return nCreateSwRenderBuffer(size)
    }

    /**
     * mpv renders directly into [buffer] (a DirectByteBuffer from [createSwRenderBuffer]).
     * outSize must be IntArray(2) to receive [width, height].
     */
    fun renderSwFrameToBuffer(buffer: Any, bufSize: Int, outSize: IntArray): Boolean {
        return nRenderSwFrameToBuffer(ptr, buffer, bufSize, outSize)
    }

    /**
     * Free the native memory backing [buffer].
     */
    fun destroySwRenderBuffer(buffer: Any) {
        nDestroySwRenderBuffer(buffer)
    }

    fun destroySwRenderContext() {
        nDestroySwRenderContext(ptr)
    }

    fun copySwPixels(outArray: ByteArray, outSize: IntArray): Boolean {
        return nCopySwPixels(ptr, outArray, outSize)
    }

    fun getSwWidth(): Int {
        return nGetSwWidth(ptr)
    }

    fun getSwHeight(): Int {
        return nGetSwHeight(ptr)
    }

    fun getVideoWidth(): Int {
        return nGetVideoWidth(ptr)
    }

    fun getVideoHeight(): Int {
        return nGetVideoHeight(ptr)
    }

    fun queryVideoResolution(outSize: IntArray): Boolean {
        return nQueryVideoResolution(ptr, outSize)
    }

    fun waitForFrame() {
        nWaitForFrame(ptr)
    }

    fun hasPendingFrame(): Boolean {
        return nHasPendingFrame(ptr)
    }

    // OpenGL render context (GPU-accelerated via WGL)

    fun createGlRenderContext(width: Int, height: Int): Boolean {
        return nCreateGlRenderContext(ptr, width, height)
    }

    fun renderGlFrame(): Boolean {
        return nRenderGlFrame(ptr)
    }

    fun destroyGlRenderContext() {
        nDestroyGlRenderContext(ptr)
    }

    fun copyGlPixels(outArray: ByteArray, outSize: IntArray): Boolean {
        return nCopyGlPixels(ptr, outArray, outSize)
    }

    fun getGlWidth(): Int {
        return nGetGlWidth(ptr)
    }

    fun getGlHeight(): Int {
        return nGetGlHeight(ptr)
    }

    // GL shared context (for GPU texture export to Skia)

    /**
     * Create a shared WGL context from mpv's GL context.
     * Returns an opaque handle (HGLRC) that can be made current on another thread
     * to access mpv's GL resources (textures, FBOs) via wglShareLists.
     * Returns 0 on failure.
     */
    fun createGlSharedContext(): Long {
        return nCreateGlSharedContext(ptr)
    }

    /**
     * Copy mpv's current FBO texture to a NEW GL texture.
     * Returns the new texture ID (Skia will take ownership via adoptTextureFrom).
     * Must be called on the render thread with the shared GL context current.
     * mpv must have already called renderGlFrame() + finishGlRender() before this.
     * outSize must be IntArray(2) to receive [width, height].
     * Returns 0 on failure.
     */
    fun copyGlTextureToNew(outSize: IntArray): Int {
        return nCopyGlTextureToNew(ptr, outSize)
    }

    /**
     * Wait for the GPU to finish mpv's last render (glFinish).
     * Call from the mpv render thread after renderGlFrame().
     */
    fun finishGlRender() {
        nFinishGlRender(ptr)
    }

    /**
     * Check if GL render context is available (created successfully).
     */
    fun isGlAvailable(): Boolean {
        return nIsGlAvailable(ptr)
    }

    /**
     * Get the GL FBO ID of mpv's render context (for Skia BackendRenderTarget).
     */
    fun getGlFboId(): Int {
        return nGetGlFboId(ptr)
    }

    // ANGLE render context (GPU-accelerated via D3D11)

    fun createAngleRenderContext(width: Int, height: Int): Boolean {
        return nCreateAngleRenderContext(ptr, width, height)
    }

    fun resizeAngleRenderContext(width: Int, height: Int): Boolean {
        return nResizeAngleRenderContext(ptr, width, height)
    }

    fun renderAngleFrame(): Boolean {
        return nRenderAngleFrame(ptr)
    }

    fun destroyAngleRenderContext() {
        nDestroyAngleRenderContext(ptr)
    }

    fun copyAnglePixels(outArray: ByteArray, outSize: IntArray): Boolean {
        return nCopyAnglePixels(ptr, outArray, outSize)
    }

    fun getAngleWidth(): Int {
        return nGetAngleWidth(ptr)
    }

    fun getAngleHeight(): Int {
        return nGetAngleHeight(ptr)
    }

    fun isAngleAvailable(): Boolean {
        return nIsAngleAvailable(ptr)
    }

    /**
     * Get the shared HANDLE of the ANGLE D3D11 texture (for cross-API GPU import).
     * Returns 0 if not using shared texture path.
     */
    fun getAngleSharedTextureHandle(): Long {
        return nGetAngleSharedTextureHandle(ptr)
    }

    /**
     * Get the ANGLE D3D11 device pointer (for direct texture access).
     */
    fun getAngleD3D11Device(): Long {
        return nGetAngleD3D11Device(ptr)
    }

    /**
     * Read pixels directly from the ANGLE D3D11 shared texture (bypasses glReadPixels).
     * Returns true on success, with width/height written to outSize.
     */
    fun readPixelsFromSharedTexture(outArray: ByteArray, outSize: IntArray): Boolean {
        return nReadPixelsFromSharedTexture(ptr, outArray, outSize)
    }

    /**
     * Async readback step 1: non-blocking GPU copy to staging texture.
     * Call this after renderAngleFrame() to start the copy pipeline.
     */
    fun beginReadPixels(): Boolean {
        return nBeginReadPixels(ptr)
    }

    /**
     * Async readback step 2: map the previous staging texture and copy to output.
     * The data is already ready on the GPU, so this is fast (~2ms).
     * Call this BEFORE the next beginReadPixels() to get the previous frame's pixels.
     */
    fun getReadPixelsResult(outArray: ByteArray, outSize: IntArray): Boolean {
        return nGetReadPixelsResult(ptr, outArray, outSize)
    }

    /**
     * Stop this `mpv_context` instance, which will run into the unrecoverable state.
     *
     * You will not expected to call any method except [close] after calling this function.
     */
    fun destroy(): Boolean {
        return nDestroy(ptr)
    }

    override fun close() {
        nFinalize(ptr)
    }

    public companion object {
        private fun createHandle(context: Any): Long {
            LibraryLoader.loadLibraries(context)
            return nMake(context)
        }

        public fun setRuntimeLibraryDirectory(path: String, extractRuntimeLibrary: Boolean = true) {
            LibraryLoader.setRuntimeLibraryDirectory(path, extractRuntimeLibrary)
        }

        public fun useDefaultRuntimeLibraryDirectory() {
            LibraryLoader.useDefaultRuntimeLibraryDirectory()
        }

        // WGL context management (GPU texture export)

        public fun setupGlContext(component: Any, hglrc: Long): Boolean {
            return nSetupGlContext(component, hglrc)
        }

        public fun setupGlContextWithHandles(mpvPtr: Long, mpvHglrc: Long): Boolean {
            return nSetupGlContextWithHandles(mpvPtr, mpvHglrc)
        }

        public fun makeContextCurrent(): Boolean {
            return nMakeContextCurrent()
        }

        public fun destroyWglContext(hglrc: Long) {
            nDestroyWglContext(hglrc)
        }

        public fun swapBuffers() {
            nSwapBuffers()
        }

        public fun wglSwapIntervalEXT(interval: Int) {
            nWglSwapIntervalEXT(interval)
        }

        public fun deleteGlTexture(textureId: Int) {
            nDeleteGlTexture(textureId)
        }

        public fun openSharedTextureOnD3D12(d3d12DevicePtr: Long, sharedHandle: Long): Long {
            return nOpenSharedTextureOnD3D12(d3d12DevicePtr, sharedHandle)
        }

        /**
         * Create a D3D12 device and command queue for GPU texture interop.
         * Returns long[3] = {adapterPtr, devicePtr, queuePtr} or null on failure.
         */
        public fun createD3D12Device(): LongArray? {
            return nCreateD3D12Device()
        }

        /**
         * Release a D3D12 resource (ID3D12Resource*) opened via [openSharedTextureOnD3D12].
         */
        public fun releaseD3D12Resource(resourcePtr: Long) {
            nReleaseD3D12Resource(resourcePtr)
        }

        /**
         * Destroy a D3D12 device created via [createD3D12Device].
         * Only releases the device; caller should release adapter and queue separately.
         */
        public fun destroyD3D12Device(devicePtr: Long) {
            nDestroyD3D12Device(devicePtr)
        }

        /**
         * Get the default DXGI adapter (IDXGIAdapter1*).
         * Used with a D3D12 device obtained from Compose/Skiko via reflection.
         */
        public fun getD3D12DefaultAdapter(): Long {
            return nGetD3D12DefaultAdapter()
        }

        /**
         * Create a D3D12 command queue on an existing device.
         * Used with Compose's D3D12 device to create a compatible DirectContext.
         */
        public fun createD3D12CommandQueue(devicePtr: Long): Long {
            return nCreateD3D12CommandQueue(devicePtr)
        }

        /**
         * Extract the raw ID3D12Device* from a Skiko struct pointer or raw device pointer.
         * The returned pointer has an extra AddRef — caller must release it when done.
         */
        public fun extractD3D12DevicePtr(skikoOrDevicePtr: Long): Long {
            return nExtractD3D12DevicePtr(skikoOrDevicePtr)
        }

        /**
         * Validate if a pointer is a valid D3D12 device.
         * Prints diagnostic info to stderr.
         */
        public fun validateD3D12Device(devicePtr: Long): Boolean {
            return nValidateD3D12Device(devicePtr)
        }

        /**
         * Transition a D3D12 resource state.
         * Needed for cross-API shared textures (D3D11→D3D12) to be properly
         * transitioned from COMMON state to PIXEL_SHADER_RESOURCE for Skia rendering.
         *
         * @param devicePtr ID3D12Device* pointer
         * @param queuePtr ID3D12CommandQueue* pointer
         * @param resourcePtr ID3D12Resource* pointer
         * @param fromState Source D3D12_RESOURCE_STATES (e.g., 0 = D3D12_RESOURCE_STATE_COMMON)
         * @param toState Target D3D12_RESOURCE_STATES (e.g., 1 = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
         */
        public fun transitionD3D12ResourceState(
            devicePtr: Long,
            queuePtr: Long,
            resourcePtr: Long,
            fromState: Int,
            toState: Int,
        ): Boolean {
            return nTransitionD3D12ResourceState(devicePtr, queuePtr, resourcePtr, fromState, toState)
        }
    }

    /*companion object {
        init { LibraryLoader.loadLibraries() }
        
        private val cleaner = Cleaner.create()
        
        private class ReferenceHolder(private val nativePtr: Long) : Runnable {
            override fun run() {  }
        }
    }*/
}

@Suppress("unused")
enum class MPVFormat {
    MPV_FORMAT_NONE,
    MPV_FORMAT_STRING,
    MPV_FORMAT_OSD_STRING,
    MPV_FORMAT_FLAG,
    MPV_FORMAT_INT64,
    MPV_FORMAT_DOUBLE,
    MPV_FORMAT_NODE,
    MPV_FORMAT_NODE_ARRAY,
    MPV_FORMAT_NODE_MAP,
    MPV_FORMAT_BYTE_ARRAY,
}

private external fun nGlobalInit(): Boolean
private external fun nMake(context: Any): Long
private external fun nInitialize(ptr: Long): Boolean
private external fun nSetEventListener(ptr: Long, eventListener: EventListener): Boolean
private external fun nCommand(ptr: Long, command: Array<out String>): Boolean
private external fun nOption(ptr: Long, key: String, value: String): Boolean
private external fun nGetPropertyInt(ptr: Long, name: String): Int
private external fun nGetPropertyBoolean(ptr: Long, name: String): Boolean
private external fun nGetPropertyDouble(ptr: Long, name: String): Double
private external fun nGetPropertyString(ptr: Long, name: String): String
private external fun nSetPropertyInt(ptr: Long, name: String, value: Int): Boolean
private external fun nSetPropertyLong(ptr: Long, name: String, value: Long): Boolean
private external fun nSetPropertyBoolean(ptr: Long, name: String, value: Boolean): Boolean
private external fun nSetPropertyStringList(ptr: Long, name: String, values: Array<String>): Boolean
private external fun nSetPropertyDouble(ptr: Long, name: String, value: Double): Boolean
private external fun nSetPropertyString(ptr: Long, name: String, value: String): Boolean
private external fun nObserveProperty(ptr: Long, name: String, format: Int, replyData: Long): Boolean
private external fun nUnobserveProperty(ptr: Long, replyData: Long): Boolean

/**
 * Attach render surface to the mpv context.
 *
 * On Android, the surface should be `android.view.Surface` object.
 */
internal expect fun attachSurface(ptr: Long, surface: Any): Boolean

/**
 * Detach current render surface of the mpv context.
 */
internal expect fun detachSurface(ptr: Long): Boolean

// Software render context native methods (vo=libmpv)
private external fun nCreateSwRenderContext(ptr: Long, width: Int, height: Int): Boolean
private external fun nResizeSwRenderContext(ptr: Long, width: Int, height: Int): Boolean
private external fun nRenderSwFrame(ptr: Long): Boolean
private external fun nCreateSwRenderBuffer(size: Int): Any
private external fun nDestroySwRenderBuffer(buffer: Any)
private external fun nRenderSwFrameToBuffer(ptr: Long, buffer: Any, bufSize: Int, outSize: IntArray): Boolean
private external fun nDestroySwRenderContext(ptr: Long)
private external fun nCopySwPixels(ptr: Long, outArray: ByteArray, outSize: IntArray): Boolean
private external fun nGetSwWidth(ptr: Long): Int
private external fun nGetSwHeight(ptr: Long): Int
private external fun nGetVideoWidth(ptr: Long): Int
private external fun nGetVideoHeight(ptr: Long): Int
private external fun nQueryVideoResolution(ptr: Long, outSize: IntArray): Boolean
private external fun nWaitForFrame(ptr: Long)
private external fun nHasPendingFrame(ptr: Long): Boolean

// OpenGL render context native methods (GPU-accelerated via WGL)
private external fun nCreateGlRenderContext(ptr: Long, width: Int, height: Int): Boolean
private external fun nRenderGlFrame(ptr: Long): Boolean
private external fun nDestroyGlRenderContext(ptr: Long)
private external fun nCopyGlPixels(ptr: Long, outArray: ByteArray, outSize: IntArray): Boolean
private external fun nGetGlWidth(ptr: Long): Int
private external fun nGetGlHeight(ptr: Long): Int

// GL shared context native methods (for GPU texture export to Skia)
private external fun nCreateGlSharedContext(ptr: Long): Long
private external fun nCopyGlTextureToNew(ptr: Long, outSize: IntArray): Int
private external fun nFinishGlRender(ptr: Long)
private external fun nIsGlAvailable(ptr: Long): Boolean
private external fun nGetGlFboId(ptr: Long): Int

// WGL context management for GPU texture export
private external fun nSetupGlContext(component: Any, hglrc: Long): Boolean
private external fun nSetupGlContextWithHandles(hdc: Long, mpvHglrc: Long): Boolean
private external fun nMakeContextCurrent(): Boolean
private external fun nDestroyWglContext(hglrc: Long)
private external fun nSwapBuffers()
private external fun nWglSwapIntervalEXT(interval: Int)
private external fun nDeleteGlTexture(textureId: Int)

// ANGLE render context native methods (GPU-accelerated via D3D11)
private external fun nCreateAngleRenderContext(ptr: Long, width: Int, height: Int): Boolean
private external fun nResizeAngleRenderContext(ptr: Long, width: Int, height: Int): Boolean
private external fun nRenderAngleFrame(ptr: Long): Boolean
private external fun nDestroyAngleRenderContext(ptr: Long)
private external fun nCopyAnglePixels(ptr: Long, outArray: ByteArray, outSize: IntArray): Boolean
private external fun nGetAngleWidth(ptr: Long): Int
private external fun nGetAngleHeight(ptr: Long): Int
private external fun nIsAngleAvailable(ptr: Long): Boolean
private external fun nGetAngleSharedTextureHandle(ptr: Long): Long
private external fun nGetAngleD3D11Device(ptr: Long): Long
private external fun nReadPixelsFromSharedTexture(ptr: Long, outArray: ByteArray, outSize: IntArray): Boolean
private external fun nBeginReadPixels(ptr: Long): Boolean
private external fun nGetReadPixelsResult(ptr: Long, outArray: ByteArray, outSize: IntArray): Boolean

// D3D12 interop: open shared HANDLE on a D3D12 device
private external fun nOpenSharedTextureOnD3D12(d3d12DevicePtr: Long, sharedHandle: Long): Long

// D3D12 interop: create device and command queue
private external fun nCreateD3D12Device(): LongArray?

// D3D12 interop: release resource and device
private external fun nReleaseD3D12Resource(resourcePtr: Long)
private external fun nDestroyD3D12Device(devicePtr: Long)

// D3D12 interop: get default adapter and create command queue (for Compose device reuse)
private external fun nGetD3D12DefaultAdapter(): Long
private external fun nCreateD3D12CommandQueue(devicePtr: Long): Long
private external fun nExtractD3D12DevicePtr(skikoOrDevicePtr: Long): Long
private external fun nValidateD3D12Device(devicePtr: Long): Boolean
private external fun nTransitionD3D12ResourceState(devicePtr: Long, queuePtr: Long, resourcePtr: Long, fromState: Int, toState: Int): Boolean

private external fun nDestroy(ptr: Long): Boolean
private external fun nFinalize(ptr: Long)
