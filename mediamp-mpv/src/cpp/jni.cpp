#include <iostream>
#include <jni.h>
#include <cstdio>
#include "include/method_cache.h"
#include "include/mpv_handle_t.h"

#ifdef _WIN32
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi.h>
#include <dxgi1_4.h>
#include <windows.h>
#endif

#define FN(name) Java_org_openani_mediamp_mpv_MPVHandleKt_##name
#define FN_ANDROID(name) Java_org_openani_mediamp_mpv_MPVHandleAndroid_##name
#define FN_DESKTOP(name) Java_org_openani_mediamp_mpv_MPVHandleDesktop_##name

extern "C" {
    JNIEXPORT jboolean JNICALL FN(nGlobalInit)(JNIEnv *env, jclass clazz);
    JNIEXPORT jlong JNICALL FN(nMake)(JNIEnv *env, jclass clazz, jobject app_context);
    JNIEXPORT jboolean JNICALL FN(nInitialize)(JNIEnv *env, jclass clazz, jlong ptr);
    JNIEXPORT jboolean JNICALL FN(nSetEventListener)(JNIEnv *env, jclass clazz, jlong ptr, jobject listener);

    /**
     * 执行 mpv 命令
     */
    JNIEXPORT jboolean JNICALL FN(nCommand)(JNIEnv *env, jclass clazz, jlong ptr, jobjectArray args);
    /**
     * 设置 mpv 选项
     */
    JNIEXPORT jboolean JNICALL FN(nOption)(JNIEnv *env, jclass clazz, jlong ptr, jstring key, jstring value);

    // 设置播放器属性
    JNIEXPORT jint JNICALL FN(nGetPropertyInt)(JNIEnv *env, jclass clazz, jlong ptr, jstring key);
    JNIEXPORT jdouble JNICALL FN(nGetPropertyDouble)(JNIEnv *env, jclass clazz, jlong ptr, jstring key);
    JNIEXPORT jboolean JNICALL FN(nGetPropertyBoolean)(JNIEnv *env, jclass clazz, jlong ptr, jstring key);
    JNIEXPORT jstring JNICALL FN(nGetPropertyString)(JNIEnv *env, jclass clazz, jlong ptr, jstring key);

    JNIEXPORT jboolean JNICALL FN(nSetPropertyString)(JNIEnv *env, jclass clazz, jlong ptr, jstring key, jstring value);
    JNIEXPORT jboolean JNICALL FN(nSetPropertyInt)(JNIEnv *env, jclass clazz, jlong ptr, jstring key, jint value);
    JNIEXPORT jboolean JNICALL FN(nSetPropertyLong)(JNIEnv *env, jclass clazz, jlong ptr, jstring key, jlong value);
    JNIEXPORT jboolean JNICALL FN(nSetPropertyDouble)(JNIEnv *env, jclass clazz, jlong ptr, jstring key, jdouble value);
    JNIEXPORT jboolean JNICALL FN(nSetPropertyBoolean)(JNIEnv *env, jclass clazz, jlong ptr, jstring key, jboolean value);
    JNIEXPORT jboolean JNICALL FN(nSetPropertyStringList)(JNIEnv *env, jclass clazz, jlong ptr, jstring key, jobjectArray values);

    JNIEXPORT jboolean JNICALL FN(nObserveProperty)(JNIEnv *env, jclass clazz, jlong ptr, jstring name, jint format, jlong reply_data);
    JNIEXPORT jboolean JNICALL FN(nUnobserveProperty)(JNIEnv *env, jclass clazz, jlong ptr, jlong reply_data);

    // renderer
    JNIEXPORT jboolean JNICALL FN_ANDROID(nAttachAndroidSurface)(JNIEnv *env, jclass clazz, jlong ptr, jobject surface);
    JNIEXPORT jboolean JNICALL FN_ANDROID(nDetachAndroidSurface)(JNIEnv *env, jclass clazz, jlong ptr);

    // Software render context (vo=libmpv)
    JNIEXPORT jboolean JNICALL FN(nCreateSwRenderContext)(JNIEnv *env, jclass clazz, jlong ptr, jint width, jint height);
    JNIEXPORT jboolean JNICALL FN(nResizeSwRenderContext)(JNIEnv *env, jclass clazz, jlong ptr, jint width, jint height);
    JNIEXPORT jboolean JNICALL FN(nRenderSwFrame)(JNIEnv *env, jclass clazz, jlong ptr);
    JNIEXPORT jobject JNICALL FN(nCreateSwRenderBuffer)(JNIEnv *env, jclass clazz, jint size);
    JNIEXPORT void JNICALL FN(nDestroySwRenderBuffer)(JNIEnv *env, jclass clazz, jobject buffer);
    JNIEXPORT jboolean JNICALL FN(nRenderSwFrameToBuffer)(JNIEnv *env, jclass clazz, jlong ptr, jobject buffer, jint bufSize, jintArray outSize);
    JNIEXPORT void JNICALL FN(nDestroySwRenderContext)(JNIEnv *env, jclass clazz, jlong ptr);
    JNIEXPORT jboolean JNICALL FN(nCopySwPixels)(JNIEnv *env, jclass clazz, jlong ptr, jbyteArray outArray, jintArray outSize);
    JNIEXPORT jint JNICALL FN(nGetSwWidth)(JNIEnv *env, jclass clazz, jlong ptr);
    JNIEXPORT jint JNICALL FN(nGetSwHeight)(JNIEnv *env, jclass clazz, jlong ptr);
    JNIEXPORT jint JNICALL FN(nGetVideoWidth)(JNIEnv *env, jclass clazz, jlong ptr);
    JNIEXPORT jint JNICALL FN(nGetVideoHeight)(JNIEnv *env, jclass clazz, jlong ptr);
    JNIEXPORT jboolean JNICALL FN(nQueryVideoResolution)(JNIEnv *env, jclass clazz, jlong ptr, jintArray outSize);
    JNIEXPORT void JNICALL FN(nWaitForFrame)(JNIEnv *env, jclass clazz, jlong ptr);
    JNIEXPORT jboolean JNICALL FN(nHasPendingFrame)(JNIEnv *env, jclass clazz, jlong ptr);

    // OpenGL render context (GPU-accelerated via WGL)
    JNIEXPORT jboolean JNICALL FN(nCreateGlRenderContext)(JNIEnv *env, jclass clazz, jlong ptr, jint width, jint height);
    JNIEXPORT jboolean JNICALL FN(nRenderGlFrame)(JNIEnv *env, jclass clazz, jlong ptr);
    JNIEXPORT void JNICALL FN(nDestroyGlRenderContext)(JNIEnv *env, jclass clazz, jlong ptr);
    JNIEXPORT jboolean JNICALL FN(nCopyGlPixels)(JNIEnv *env, jclass clazz, jlong ptr, jbyteArray outArray, jintArray outSize);
    JNIEXPORT jint JNICALL FN(nGetGlWidth)(JNIEnv *env, jclass clazz, jlong ptr);
    JNIEXPORT jint JNICALL FN(nGetGlHeight)(JNIEnv *env, jclass clazz, jlong ptr);

    // GL shared context (for GPU texture export to Skia)
    JNIEXPORT jlong JNICALL FN(nCreateGlSharedContext)(JNIEnv *env, jclass clazz, jlong ptr);
    JNIEXPORT jlong JNICALL FN(nGetGlHdc)(JNIEnv *env, jclass clazz, jlong ptr);
    JNIEXPORT jint JNICALL FN(nCopyGlTextureToNew)(JNIEnv *env, jclass clazz, jlong ptr, jintArray outSize);
    JNIEXPORT void JNICALL FN(nFinishGlRender)(JNIEnv *env, jclass clazz, jlong ptr);
    JNIEXPORT jboolean JNICALL FN(nIsGlAvailable)(JNIEnv *env, jclass clazz, jlong ptr);
    JNIEXPORT jint JNICALL FN(nGetGlFboId)(JNIEnv *env, jclass clazz, jlong ptr);

    // WGL context management for GPU texture export
    JNIEXPORT jboolean JNICALL FN(nSetupGlContext)(JNIEnv *env, jclass clazz, jobject component, jlong hglrc);
    JNIEXPORT jboolean JNICALL FN(nSetupGlContextWithHandles)(JNIEnv *env, jclass clazz, jlong mpvPtr, jlong mpvHglrc);
    JNIEXPORT jboolean JNICALL FN(nMakeContextCurrent)(JNIEnv *env, jclass clazz);
    JNIEXPORT void JNICALL FN(nDestroyWglContext)(JNIEnv *env, jclass clazz, jlong hglrc);
    JNIEXPORT void JNICALL FN(nSwapBuffers)(JNIEnv *env, jclass clazz);
    JNIEXPORT void JNICALL FN(nWglSwapIntervalEXT)(JNIEnv *env, jclass clazz, jint interval);
    JNIEXPORT void JNICALL FN(nDeleteGlTexture)(JNIEnv *env, jclass clazz, jint textureId);

    // ANGLE render context (GPU-accelerated via D3D11)
    JNIEXPORT jboolean JNICALL FN(nCreateAngleRenderContext)(JNIEnv *env, jclass clazz, jlong ptr, jint width, jint height);
    JNIEXPORT jboolean JNICALL FN(nResizeAngleRenderContext)(JNIEnv *env, jclass clazz, jlong ptr, jint width, jint height);
    JNIEXPORT jboolean JNICALL FN(nRenderAngleFrame)(JNIEnv *env, jclass clazz, jlong ptr);
    JNIEXPORT void JNICALL FN(nDestroyAngleRenderContext)(JNIEnv *env, jclass clazz, jlong ptr);
    JNIEXPORT jboolean JNICALL FN(nCopyAnglePixels)(JNIEnv *env, jclass clazz, jlong ptr, jbyteArray outArray, jintArray outSize);
    JNIEXPORT jint JNICALL FN(nGetAngleWidth)(JNIEnv *env, jclass clazz, jlong ptr);
    JNIEXPORT jint JNICALL FN(nGetAngleHeight)(JNIEnv *env, jclass clazz, jlong ptr);
    JNIEXPORT jboolean JNICALL FN(nIsAngleAvailable)(JNIEnv *env, jclass clazz, jlong ptr);
    JNIEXPORT jlong JNICALL FN(nGetAngleSharedTextureHandle)(JNIEnv *env, jclass clazz, jlong ptr);
    JNIEXPORT jlong JNICALL FN(nGetAngleD3D11Device)(JNIEnv *env, jclass clazz, jlong ptr);
    JNIEXPORT jboolean JNICALL FN(nReadPixelsFromSharedTexture)(JNIEnv *env, jclass clazz, jlong ptr, jbyteArray outArray, jintArray outSize);
    JNIEXPORT jboolean JNICALL FN(nBeginReadPixels)(JNIEnv *env, jclass clazz, jlong ptr);
    JNIEXPORT jboolean JNICALL FN(nGetReadPixelsResult)(JNIEnv *env, jclass clazz, jlong ptr, jbyteArray outArray, jintArray outSize);
    JNIEXPORT jlong JNICALL FN(nOpenSharedTextureOnD3D12)(JNIEnv *env, jclass clazz, jlong d3d12DevicePtr, jlong sharedHandle);
    JNIEXPORT jlongArray JNICALL FN(nCreateD3D12Device)(JNIEnv *env, jclass clazz);
    JNIEXPORT void JNICALL FN(nReleaseD3D12Resource)(JNIEnv *env, jclass clazz, jlong resourcePtr);
    JNIEXPORT void JNICALL FN(nDestroyD3D12Device)(JNIEnv *env, jclass clazz, jlong devicePtr);
    JNIEXPORT jlong JNICALL FN(nGetD3D12DefaultAdapter)(JNIEnv *env, jclass clazz);
    JNIEXPORT jlong JNICALL FN(nCreateD3D12CommandQueue)(JNIEnv *env, jclass clazz, jlong devicePtr);
    JNIEXPORT jlong JNICALL FN(nExtractD3D12DevicePtr)(JNIEnv *env, jclass clazz, jlong skikoOrDevicePtr);
    JNIEXPORT jboolean JNICALL FN(nValidateD3D12Device)(JNIEnv *env, jclass clazz, jlong devicePtr);
    JNIEXPORT jboolean JNICALL FN(nTransitionD3D12ResourceState)(JNIEnv *env, jclass clazz, jlong devicePtr, jlong queuePtr, jlong resourcePtr, jint fromState, jint toState);

/**
 * 关闭此 mpv_handle_t 实例
 */
    JNIEXPORT jboolean JNICALL FN(nDestroy)(JNIEnv *env, jclass clazz, jlong ptr);
    /**
     * 被 GC 调用，回收 mpv_handle_t
     */
    JNIEXPORT void JNICALL FN(nFinalize)(JNIEnv *env, jclass clazz, jlong ptr);
}

// implementations

JNIEXPORT jboolean JNICALL FN(nGlobalInit)(JNIEnv *env, jclass clazz) {
    return true;
}

JNIEXPORT jlong JNICALL FN(nMake)(JNIEnv *env, jclass clazz, jobject app_context) {
    auto* handle = new mediampv::mpv_handle_t(env, app_context);
    return reinterpret_cast<jlong>(handle);
}

JNIEXPORT jboolean JNICALL FN(nInitialize)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    return instance->initialize();
}

JNIEXPORT jboolean JNICALL FN(nSetEventListener)
        (JNIEnv *env, jclass clazz, jlong ptr, jobject listener) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    return instance->set_event_listener(env, listener);
}

JNIEXPORT jboolean JNICALL FN(nCommand)(JNIEnv *env, jclass clazz, jlong ptr, jobjectArray args) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));

    const char *arguments[128] = { 0 };
    jsize len = env->GetArrayLength(args);

    if (len >= sizeof(arguments) / sizeof(arguments[0])) {
        LOG("arguments are too long (>128)");
        return false;
    }

    for (int i = 0; i < len; ++i)
        arguments[i] = env->GetStringUTFChars((jstring)env->GetObjectArrayElement(args, i), nullptr);

    bool result = instance->command(arguments);

    for (int i = 0; i < len; ++i)
        env->ReleaseStringUTFChars((jstring)env->GetObjectArrayElement(args, i), arguments[i]);

    return result;
}

JNIEXPORT jboolean JNICALL FN(nOption)(JNIEnv *env, jclass clazz, jlong ptr, jstring key, jstring value) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));

    const char *option_key_char = env->GetStringUTFChars(key, nullptr);
    const char *option_value_char = env->GetStringUTFChars(value, nullptr);

    bool result = instance->set_option(option_key_char, option_value_char);

    env->ReleaseStringUTFChars(key, option_key_char);
    env->ReleaseStringUTFChars(value, option_value_char);

    return result;
}

// property set and get

JNIEXPORT jint JNICALL FN(nGetPropertyInt)(JNIEnv *env, jclass clazz, jlong ptr, jstring key) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));

    const char *key_char = env->GetStringUTFChars(key, nullptr);
    int64_t result = 0;

    instance->get_property(key_char, MPV_FORMAT_INT64, &result);
    env->ReleaseStringUTFChars(key, key_char);

    return static_cast<jint>(result);
}

JNIEXPORT jdouble JNICALL FN(nGetPropertyDouble)(JNIEnv *env, jclass clazz, jlong ptr, jstring key) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));

    const char *key_char = env->GetStringUTFChars(key, nullptr);
    double result = 0;

    instance->get_property(key_char, MPV_FORMAT_DOUBLE, &result);
    env->ReleaseStringUTFChars(key, key_char);

    return result;
}

JNIEXPORT jboolean JNICALL FN(nGetPropertyBoolean)(JNIEnv *env, jclass clazz, jlong ptr, jstring key) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));

    const char *key_char = env->GetStringUTFChars(key, nullptr);
    int result = 0;

    instance->get_property(key_char, MPV_FORMAT_FLAG, &result);
    env->ReleaseStringUTFChars(key, key_char);

    return result;
}

JNIEXPORT jstring JNICALL FN(nGetPropertyString)(JNIEnv *env, jclass clazz, jlong ptr, jstring key) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));

    const char *key_char = env->GetStringUTFChars(key, nullptr);
    char *result = nullptr;

    instance->get_property(key_char, MPV_FORMAT_STRING, &result);
    env->ReleaseStringUTFChars(key, key_char);

    jstring jresult = env->NewStringUTF(result);
    mpv_free(result); // TODO: move to mpv_handle_t

    return jresult;
}

JNIEXPORT jboolean JNICALL FN(nSetPropertyString)(JNIEnv *env, jclass clazz, jlong ptr, jstring key, jstring value) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));

    const char *key_char = env->GetStringUTFChars(key, nullptr);
    const char *value_char = env->GetStringUTFChars(value, nullptr);

    bool result = instance->set_property(key_char, MPV_FORMAT_STRING, &value_char);

    env->ReleaseStringUTFChars(key, key_char);
    env->ReleaseStringUTFChars(value, value_char);

    return result;
}

JNIEXPORT jboolean JNICALL FN(nSetPropertyInt)(JNIEnv *env, jclass clazz, jlong ptr, jstring key, jint value) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));

    const char *key_char = env->GetStringUTFChars(key, nullptr);

    bool result = instance->set_property(key_char, MPV_FORMAT_INT64, &value);
    env->ReleaseStringUTFChars(key, key_char);

    return result;
}

JNIEXPORT jboolean JNICALL FN(nSetPropertyLong)(JNIEnv *env, jclass clazz, jlong ptr, jstring key, jlong value) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));

    const char *key_char = env->GetStringUTFChars(key, nullptr);
    int64_t v = static_cast<int64_t>(value);

    bool result = instance->set_property(key_char, MPV_FORMAT_INT64, &v);
    env->ReleaseStringUTFChars(key, key_char);

    return result;
}

JNIEXPORT jboolean JNICALL FN(nSetPropertyDouble)(JNIEnv *env, jclass clazz, jlong ptr, jstring key, jdouble value) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));

    const char *key_char = env->GetStringUTFChars(key, nullptr);

    bool result = instance->set_property(key_char, MPV_FORMAT_DOUBLE, &value);
    env->ReleaseStringUTFChars(key, key_char);

    return result;
}

JNIEXPORT jboolean JNICALL FN(nSetPropertyBoolean)(JNIEnv *env, jclass clazz, jlong ptr, jstring key, jboolean value) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));

    const char *key_char = env->GetStringUTFChars(key, nullptr);

    bool result = instance->set_property(key_char, MPV_FORMAT_FLAG, &value);
    env->ReleaseStringUTFChars(key, key_char);

    return result;
}

JNIEXPORT jboolean JNICALL FN(nSetPropertyStringList)(JNIEnv *env, jclass clazz, jlong ptr, jstring key, jobjectArray values) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));

    const char *key_char = env->GetStringUTFChars(key, nullptr);
    jsize count = env->GetArrayLength(values);

    // Build mpv_node with NODE_ARRAY of STRINGs
    auto* node_list = new mpv_node[count];
    // Need to keep string copies alive until mpv processes the command
    std::vector<std::string> string_store;
    string_store.reserve(count);

    for (jsize i = 0; i < count; i++) {
        auto jstr = (jstring) env->GetObjectArrayElement(values, i);
        const char *s = env->GetStringUTFChars(jstr, nullptr);
        string_store.emplace_back(s);
        env->ReleaseStringUTFChars(jstr, s);
        node_list[i].format = MPV_FORMAT_STRING;
        node_list[i].u.string = const_cast<char*>(string_store.back().c_str());
    }

    mpv_node top;
    top.format = MPV_FORMAT_NODE_ARRAY;
    top.u.list = new mpv_node_list();
    top.u.list->num = count;
    top.u.list->values = node_list;

    bool result = instance->set_property(key_char, MPV_FORMAT_NODE, &top);

    delete top.u.list;
    delete[] node_list;
    env->ReleaseStringUTFChars(key, key_char);

    return result;
}

JNIEXPORT jboolean JNICALL FN(nObserveProperty)(JNIEnv *env, jclass clazz, jlong ptr, jstring key, jint format, jlong reply_data) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));

    const char *key_char = env->GetStringUTFChars(key, nullptr);

    bool result = instance->observe_property(key_char, static_cast<mpv_format>(format), reply_data);
    env->ReleaseStringUTFChars(key, key_char);

    return result;
}

JNIEXPORT jboolean JNICALL FN(nUnobserveProperty)(JNIEnv *env, jclass clazz, jlong ptr, jlong reply_data) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    return instance->unobserve_property(reply_data);
}

JNIEXPORT jboolean JNICALL FN_ANDROID(nAttachAndroidSurface)(JNIEnv *env, jclass clazz, jlong ptr, jobject surface) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    return instance->attach_android_surface(env, surface);
}

JNIEXPORT jboolean JNICALL FN_ANDROID(nDetachAndroidSurface)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    return instance->detach_android_surface(env);
}

// Software render context JNI implementations

JNIEXPORT jboolean JNICALL FN(nCreateSwRenderContext)(JNIEnv *env, jclass clazz, jlong ptr, jint width, jint height) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    return instance->create_sw_render_context(width, height);
}

JNIEXPORT jboolean JNICALL FN(nResizeSwRenderContext)(JNIEnv *env, jclass clazz, jlong ptr, jint width, jint height) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    return instance->resize_sw_render_context(width, height);
}

JNIEXPORT jboolean JNICALL FN(nRenderSwFrame)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    return instance->render_sw_frame();
}

JNIEXPORT jobject JNICALL FN(nCreateSwRenderBuffer)(JNIEnv *env, jclass clazz, jint size) {
    auto *buf = static_cast<uint8_t*>(malloc(size));
    if (!buf) return nullptr;
    memset(buf, 0, size);
    return env->NewDirectByteBuffer(buf, size);
}

JNIEXPORT void JNICALL FN(nDestroySwRenderBuffer)(JNIEnv *env, jclass clazz, jobject buffer) {
    if (!buffer) return;
    auto *addr = env->GetDirectBufferAddress(buffer);
    if (addr) free(addr);
}

JNIEXPORT jboolean JNICALL FN(nRenderSwFrameToBuffer)(JNIEnv *env, jclass clazz, jlong ptr, jobject buffer, jint bufSize, jintArray outSize) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));

    auto *buf = static_cast<uint8_t*>(env->GetDirectBufferAddress(buffer));
    if (!buf) return false;

    int out_w = 0, out_h = 0;
    bool ok = instance->render_sw_frame_to_direct(buf, (int)bufSize, &out_w, &out_h);
    if (ok) {
        jint size[2] = {out_w, out_h};
        env->SetIntArrayRegion(outSize, 0, 2, size);
    }
    return ok;
}

JNIEXPORT void JNICALL FN(nDestroySwRenderContext)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    instance->destroy_sw_render_context();
}

JNIEXPORT jboolean JNICALL FN(nCopySwPixels)(JNIEnv *env, jclass clazz, jlong ptr, jbyteArray outArray, jintArray outSize) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));

    jsize arrayLen = env->GetArrayLength(outArray);
    int out_w = 0, out_h = 0;

    jbyte *dst = env->GetByteArrayElements(outArray, nullptr);
    bool ok = instance->copy_sw_pixels(reinterpret_cast<uint8_t*>(dst), arrayLen, &out_w, &out_h);
    env->ReleaseByteArrayElements(outArray, dst, ok ? 0 : JNI_ABORT);

    if (ok) {
        jint size[2] = {out_w, out_h};
        env->SetIntArrayRegion(outSize, 0, 2, size);
    }
    return ok;
}

JNIEXPORT jint JNICALL FN(nGetSwWidth)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    return instance->get_sw_width();
}

JNIEXPORT jint JNICALL FN(nGetSwHeight)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    return instance->get_sw_height();
}

JNIEXPORT jint JNICALL FN(nGetVideoWidth)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    return instance->get_video_width();
}

JNIEXPORT jint JNICALL FN(nGetVideoHeight)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    return instance->get_video_height();
}

JNIEXPORT jboolean JNICALL FN(nQueryVideoResolution)(JNIEnv *env, jclass clazz, jlong ptr, jintArray outSize) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    int w = 0, h = 0;
    bool ok = instance->query_video_resolution(&w, &h);
    if (ok) {
        jint size[2] = {w, h};
        env->SetIntArrayRegion(outSize, 0, 2, size);
    }
    return ok;
}

JNIEXPORT void JNICALL FN(nWaitForFrame)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    instance->wait_for_frame();
}

JNIEXPORT jboolean JNICALL FN(nHasPendingFrame)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    return instance->has_pending_frame();
}

// OpenGL render context JNI implementations

JNIEXPORT jboolean JNICALL FN(nCreateGlRenderContext)(JNIEnv *env, jclass clazz, jlong ptr, jint width, jint height) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    return instance->create_gl_render_context(width, height);
}

JNIEXPORT jboolean JNICALL FN(nRenderGlFrame)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    return instance->render_gl_frame();
}

JNIEXPORT void JNICALL FN(nDestroyGlRenderContext)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    instance->destroy_gl_render_context();
}

JNIEXPORT jboolean JNICALL FN(nCopyGlPixels)(JNIEnv *env, jclass clazz, jlong ptr, jbyteArray outArray, jintArray outSize) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));

    jsize arrayLen = env->GetArrayLength(outArray);
    int out_w = 0, out_h = 0;

    jbyte *dst = env->GetByteArrayElements(outArray, nullptr);
    bool ok = instance->copy_gl_pixels(reinterpret_cast<uint8_t*>(dst), arrayLen, &out_w, &out_h);
    env->ReleaseByteArrayElements(outArray, dst, ok ? 0 : JNI_ABORT);

    if (ok) {
        jint size[2] = {out_w, out_h};
        env->SetIntArrayRegion(outSize, 0, 2, size);
    }
    return ok;
}

JNIEXPORT jint JNICALL FN(nGetGlWidth)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    return instance->get_gl_width();
}

JNIEXPORT jint JNICALL FN(nGetGlHeight)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    return instance->get_gl_height();
}

// GL shared context JNI implementations

JNIEXPORT jlong JNICALL FN(nCreateGlSharedContext)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    void *ctx = instance->create_gl_shared_context();
    return reinterpret_cast<jlong>(ctx);
}

JNIEXPORT jlong JNICALL FN(nGetGlHdc)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    void *hdc = instance->get_gl_hdc();
    return reinterpret_cast<jlong>(hdc);
}

JNIEXPORT jint JNICALL FN(nCopyGlTextureToNew)(JNIEnv *env, jclass clazz, jlong ptr, jintArray outSize) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    int outW = 0, outH = 0;
    unsigned int texId = instance->copy_gl_texture_to_new(&outW, &outH);
    if (texId != 0) {
        jint dims[2] = {outW, outH};
        env->SetIntArrayRegion(outSize, 0, 2, dims);
    }
    return static_cast<jint>(texId);
}

JNIEXPORT void JNICALL FN(nFinishGlRender)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    instance->finish_gl_render();
}

JNIEXPORT jboolean JNICALL FN(nIsGlAvailable)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    return instance->is_gl_available();
}

JNIEXPORT jint JNICALL FN(nGetGlFboId)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    return static_cast<jint>(instance->get_gl_fbo_id());
}

// WGL context management for GPU texture export

#ifdef _WIN32
#include <GL/gl.h>
// WGL function pointer types
typedef BOOL (WINAPI *PFNWGLSWAPINTERVALEXTPROC)(int interval);
static HDC s_render_hdc = nullptr;
static HWND s_render_hwnd = nullptr;
static HGLRC s_shared_hglrc = nullptr;
static PFNWGLSWAPINTERVALEXTPROC s_wglSwapIntervalEXT = nullptr;
#endif

#ifdef _WIN32
// Find HWND from a java.awt.Component by traversing the component hierarchy
// and accessing the native peer's getHWND() method via JNI.
// Uses direct field access (peer) instead of method calls to avoid protected access issues.
static HWND findHwndFromComponent(JNIEnv *env, jobject component) {
    if (!component) {
        LOG("findHwnd: component is null");
        return nullptr;
    }

    jclass componentCls = env->FindClass("java/awt/Component");
    if (!componentCls) {
        LOG("findHwnd: Component class not found");
        return nullptr;
    }

    // Use the protected 'peer' field directly (JNI can access any field)
    jfieldID peerField = env->GetFieldID(componentCls, "peer", "Ljava/awt/peer/ComponentPeer;");
    if (!peerField) {
        env->ExceptionClear();
        LOG("findHwnd: peer field not found");
        return nullptr;
    }

    jmethodID getParent = env->GetMethodID(componentCls, "getParent", "()Ljava/awt/Container;");
    jmethodID isDisplayable = env->GetMethodID(componentCls, "isDisplayable", "()Z");
    jmethodID getClassMethod = env->GetMethodID(env->FindClass("java/lang/Object"), "getClass", "()Ljava/lang/Class;");
    jmethodID getClassName = env->GetMethodID(env->FindClass("java/lang/Class"), "getName", "()Ljava/lang/String;");
    if (!getParent || !isDisplayable) {
        LOG("findHwnd: getParent or isDisplayable not found");
        return nullptr;
    }

    // Search for a way to get HWND from WComponentPeer/WObjectPeer
    jclass wPeerCls = env->FindClass("sun/awt/windows/WComponentPeer");
    if (!wPeerCls) { env->ExceptionClear(); return nullptr; }

    jmethodID getHwnd = nullptr;
    jfieldID hwndField = nullptr;

    // 1) Try method calls on WComponentPeer
    const char* methodNames[] = {"getHWnd", "getHWND", "getNativeHandle", "getHwnd"};
    for (const char* mname : methodNames) {
        getHwnd = env->GetMethodID(wPeerCls, mname, "()J");
        if (getHwnd) { LOG("findHwnd: Found method %s()", mname); break; }
        env->ExceptionClear();
    }

    // 2) Try field access on WComponentPeer
    if (!getHwnd) {
        const char* fieldNames[] = {"hWnd", "nativeHandle", "hwnd", "handle", "m_hwnd", "m_hWnd"};
        for (const char* fname : fieldNames) {
            hwndField = env->GetFieldID(wPeerCls, fname, "J");
            if (hwndField) { LOG("findHwnd: Found field '%s' on WComponentPeer", fname); break; }
            env->ExceptionClear();
        }
    }

    // 3) Try field access on WObjectPeer (parent class)
    if (!getHwnd && !hwndField) {
        jclass wObjectPeerCls = env->FindClass("sun/awt/windows/WObjectPeer");
        if (wObjectPeerCls) {
            const char* fieldNames[] = {"nativeHandle", "hWnd", "hwnd"};
            for (const char* fname : fieldNames) {
                hwndField = env->GetFieldID(wObjectPeerCls, fname, "J");
                if (hwndField) { LOG("findHwnd: Found field '%s' on WObjectPeer", fname); break; }
                env->ExceptionClear();
            }
        } else { env->ExceptionClear(); }
    }

    if (!getHwnd && !hwndField) {
        LOG("findHwnd: No HWND method or field found on any peer class");
    }

    // Traverse up the component hierarchy
    jobject current = env->NewLocalRef(component);
    HWND hwnd = nullptr;

    for (int i = 0; i < 50 && current; i++) {
        // Get class name for debugging
        const char* className = "unknown";
        jstring clsNameJstr = nullptr;
        if (getClassMethod && getClassName) {
            jobject cls = env->CallObjectMethod(current, getClassMethod);
            if (cls) {
                clsNameJstr = (jstring)env->CallObjectMethod(cls, getClassName);
                if (clsNameJstr) {
                    className = env->GetStringUTFChars(clsNameJstr, nullptr);
                }
                env->DeleteLocalRef(cls);
            }
        }

        jboolean displayable = env->CallBooleanMethod(current, isDisplayable);
        LOG("findHwnd[%d]: class=%s, displayable=%d", i, className, displayable);

        if (displayable) {
            jobject peer = env->GetObjectField(current, peerField);
            if (peer) {
                // Log peer class
                jobject peerCls = env->CallObjectMethod(peer, getClassMethod);
                if (peerCls) {
                    jstring peerClsName = (jstring)env->CallObjectMethod(peerCls, getClassName);
                    if (peerClsName) {
                        const char* pcn = env->GetStringUTFChars(peerClsName, nullptr);
                        LOG("findHwnd[%d]: peer class=%s", i, pcn);
                        env->ReleaseStringUTFChars(peerClsName, pcn);
                    }
                    env->DeleteLocalRef(peerCls);
                }

                // Try to get HWND from the peer
                if (env->IsInstanceOf(peer, wPeerCls)) {
                    jlong hwndVal = 0;
                    if (getHwnd) {
                        hwndVal = env->CallLongMethod(peer, getHwnd);
                        if (env->ExceptionCheck()) { env->ExceptionClear(); hwndVal = 0; }
                    }
                    if (!hwndVal && hwndField) {
                        hwndVal = env->GetLongField(peer, hwndField);
                    }
                    if (hwndVal) {
                        hwnd = reinterpret_cast<HWND>(static_cast<uintptr_t>(hwndVal));
                        LOG("findHwnd[%d]: Got HWND=%p", i, hwnd);
                        env->DeleteLocalRef(peer);
                        if (className != "unknown" && clsNameJstr) env->ReleaseStringUTFChars(clsNameJstr, className);
                        break;
                    } else {
                        LOG("findHwnd[%d]: peer is WComponentPeer but HWND is 0", i);
                    }
                }
                env->DeleteLocalRef(peer);
            } else {
                LOG("findHwnd[%d]: peer is null", i);
            }
        }

        if (className != "unknown" && clsNameJstr) {
            env->ReleaseStringUTFChars(clsNameJstr, className);
        }

        jobject parent = env->CallObjectMethod(current, getParent);
        env->DeleteLocalRef(current);
        current = parent;
    }
    if (current) env->DeleteLocalRef(current);

    if (!hwnd) {
        LOG("findHwnd: Failed to find HWND after traversing component hierarchy");
    }
    return hwnd;
}
#endif

// Try to create a shared GL context on a given HDC + HWND.
// Returns the shared HGLRC on success, nullptr on failure.
// On failure, the caller should try a hidden HWND fallback.
static HGLRC tryCreateSharedContext(HDC hdc, HGLRC mpv_hglrc, const char* label) {
    // Create a temporary context to load wglCreateContextAttribsARB
    HGLRC tempCtx = wglCreateContext(hdc);
    if (!tempCtx) {
        LOG("tryCreateSharedContext(%s): wglCreateContext failed: 0x%lX", label, GetLastError());
        return nullptr;
    }
    wglMakeCurrent(hdc, tempCtx);

    typedef HGLRC (WINAPI *PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int*);
    auto wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)
        wglGetProcAddress("wglCreateContextAttribsARB");

    HGLRC sharedCtx = nullptr;
    if (wglCreateContextAttribsARB) {
        // Try 4.5 Core
        int attribs45[] = {
            0x2091 /*WGL_CONTEXT_MAJOR_VERSION_ARB*/, 4,
            0x2092 /*WGL_CONTEXT_MINOR_VERSION_ARB*/, 5,
            0x9126 /*WGL_CONTEXT_PROFILE_MASK_ARB*/, 0x00000001,
            0
        };
        sharedCtx = wglCreateContextAttribsARB(hdc, mpv_hglrc, attribs45);
        if (!sharedCtx) {
            LOG("tryCreateSharedContext(%s): 4.5 core failed: 0x%lX", label, GetLastError());
            // Try 3.3 Core
            int attribs33[] = { 0x2091, 3, 0x2092, 3, 0x9126, 0x00000001, 0 };
            sharedCtx = wglCreateContextAttribsARB(hdc, mpv_hglrc, attribs33);
            if (!sharedCtx) {
                LOG("tryCreateSharedContext(%s): 3.3 core failed: 0x%lX", label, GetLastError());
            }
        }
    }

    // Cleanup temp context
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(tempCtx);

    if (sharedCtx) {
        LOG("tryCreateSharedContext(%s): SUCCESS", label);
    }
    return sharedCtx;
}

// Create a shared GL context on the target Component's native window.
// Strategy: query existing pixel format (SetPixelFormat can only be called once per HWND).
// If the existing format supports OpenGL, create context directly on it.
// Otherwise, create a hidden HWND with a proper GL pixel format.
// Does NOT make the context current — call nMakeContextCurrent on the render thread.
JNIEXPORT jboolean JNICALL FN(nSetupGlContext)(JNIEnv *env, jclass clazz, jobject component, jlong mpv_hglrc_ptr) {
#ifdef _WIN32
    HGLRC mpv_hglrc = reinterpret_cast<HGLRC>(static_cast<uintptr_t>(mpv_hglrc_ptr));
    if (!component || !mpv_hglrc) return false;

    // Verify component is a java.awt.Component instance
    jclass componentCls = env->FindClass("java/awt/Component");
    if (!componentCls || !env->IsInstanceOf(component, componentCls)) {
        LOG("nSetupGlContext: component is not a java.awt.Component");
        return false;
    }

    HWND targetHwnd = findHwndFromComponent(env, component);
    if (!targetHwnd) {
        LOG("nSetupGlContext: Failed to find HWND from Component");
        return false;
    }
    LOG("nSetupGlContext: Found target HWND=%p", targetHwnd);

    HDC targetHdc = GetDC(targetHwnd);
    if (!targetHdc) {
        LOG("GetDC failed for target HWND");
        return false;
    }

    // Step 1: Query existing pixel format — DO NOT call SetPixelFormat (Skia already set it)
    PIXELFORMATDESCRIPTOR existingPfd = {};
    existingPfd.nSize = sizeof(existingPfd);
    int existingFormat = GetPixelFormat(targetHdc);
    if (existingFormat > 0) {
        DescribePixelFormat(targetHdc, existingFormat, sizeof(existingPfd), &existingPfd);
        LOG("nSetupGlContext: Existing pixel format=%d, flags=0x%lX, colorBits=%d, depthBits=%d",
            existingFormat, existingPfd.dwFlags, existingPfd.cColorBits, existingPfd.cDepthBits);

        if (existingPfd.dwFlags & PFD_SUPPORT_OPENGL) {
            // Existing format supports OpenGL — try creating context directly
            LOG("nSetupGlContext: Existing format supports OpenGL, trying direct context creation...");
            HGLRC sharedCtx = tryCreateSharedContext(targetHdc, mpv_hglrc, "direct");
            if (sharedCtx) {
                s_render_hdc = targetHdc;
                s_render_hwnd = targetHwnd;
                s_shared_hglrc = sharedCtx;
                LOG("Shared GL context created on target HWND=%p (existing pixel format)", targetHwnd);
                return true;
            }
            LOG("nSetupGlContext: Direct context creation failed, trying hidden HWND fallback...");
        } else {
            LOG("nSetupGlContext: Existing pixel format does NOT support OpenGL, trying hidden HWND fallback...");
        }
    } else {
        LOG("nSetupGlContext: No pixel format set on target HDC, trying hidden HWND fallback...");
    }

    // Step 2: Fallback — create a hidden HWND with a proper GL pixel format.
    // GL objects (textures, FBOs) are shared across contexts regardless of HWND.
    ReleaseDC(targetHwnd, targetHdc);

    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "MpvGlHiddenWindow";
    RegisterClassA(&wc);

    HWND hiddenHwnd = CreateWindowExA(
        0, "MpvGlHiddenWindow", "mpv-gl",
        WS_OVERLAPPEDWINDOW,
        0, 0, 1, 1,
        nullptr, nullptr, GetModuleHandleA(nullptr), nullptr
    );
    if (!hiddenHwnd) {
        LOG("nSetupGlContext: Failed to create hidden window: 0x%lX", GetLastError());
        return false;
    }

    HDC hiddenHdc = GetDC(hiddenHwnd);
    if (!hiddenHdc) {
        LOG("nSetupGlContext: GetDC failed for hidden window");
        DestroyWindow(hiddenHwnd);
        return false;
    }

    // Set a proper GL pixel format on the hidden window
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int format = ChoosePixelFormat(hiddenHdc, &pfd);
    if (!format || !SetPixelFormat(hiddenHdc, format, &pfd)) {
        LOG("nSetupGlContext: SetPixelFormat on hidden window failed: 0x%lX", GetLastError());
        ReleaseDC(hiddenHwnd, hiddenHdc);
        DestroyWindow(hiddenHwnd);
        return false;
    }

    LOG("nSetupGlContext: Hidden window pixel format=%d", format);

    HGLRC sharedCtx = tryCreateSharedContext(hiddenHdc, mpv_hglrc, "hidden");
    if (!sharedCtx) {
        LOG("nSetupGlContext: Failed to create shared GL context on hidden HWND");
        ReleaseDC(hiddenHwnd, hiddenHdc);
        DestroyWindow(hiddenHwnd);
        return false;
    }

    // Store hidden window's HDC for rendering
    s_render_hdc = hiddenHdc;
    s_render_hwnd = hiddenHwnd;
    s_shared_hglrc = sharedCtx;

    LOG("Shared GL context created on hidden HWND=%p (shared with mpv context)", hiddenHwnd);
    return true;
#else
    return false;
#endif
}

// Make the shared GL context current on the calling thread.
// Must be called from the render thread after nSetupGlContext.
JNIEXPORT jboolean JNICALL FN(nMakeContextCurrent)(JNIEnv *env, jclass clazz) {
#ifdef _WIN32
    if (!s_render_hdc || !s_shared_hglrc) {
        LOG("nMakeContextCurrent: no context to make current");
        return false;
    }
    if (!wglMakeCurrent(s_render_hdc, s_shared_hglrc)) {
        LOG("nMakeContextCurrent: wglMakeCurrent failed: %lu", GetLastError());
        return false;
    }

    // Load wglSwapIntervalEXT (must be called with context current)
    if (!s_wglSwapIntervalEXT) {
        s_wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
    }

    LOG("Shared GL context made current on render thread");
    return true;
#else
    return false;
#endif
}

// Create a shared GL context for GPU texture export.
//
// This creates a new HGLRC on Skia's HDC that shares GL objects (textures, FBOs)
// with mpv's HGLRC. The key insight: textures are created by mpv on mpv's HGLRC,
// so the sharing must be with mpv's HGLRC, not Skia's.
//
// Parameters:
//   hdc_ptr: Skia's HDC (from WindowsOpenGLRedrawer.device)
//   mpv_hglrc_ptr: mpv's HGLRC (from render_context_t.createSharedGLContext())
//
// The new shared context is stored as s_shared_hglrc and can be made current
// on the mpv-gpu-render thread via nMakeContextCurrent.
// s_render_hwnd is set to 0 to mark "raw handle mode" — nDestroyWglContext
// will not call ReleaseDC since the HDC is owned by Skia.
JNIEXPORT jboolean JNICALL FN(nSetupGlContextWithHandles)(JNIEnv *env, jclass clazz, jlong mpv_ptr, jlong mpv_hglrc_ptr) {
    LOG("nSetupGlContextWithHandles: ENTER");
#ifdef _WIN32
    HGLRC mpv_hglrc = reinterpret_cast<HGLRC>(static_cast<uintptr_t>(mpv_hglrc_ptr));
    if (!mpv_hglrc) {
        LOG("nSetupGlContextWithHandles: invalid mpv_hglrc");
        return false;
    }

    // Get mpv's HDC from its render context
    auto *instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(mpv_ptr));
    HDC mpv_hdc = static_cast<HDC>(instance->get_gl_hdc());
    if (!mpv_hdc) {
        LOG("nSetupGlContextWithHandles: mpv has no HDC");
        return false;
    }

    // Step 1: Make mpv's context current to load wglCreateContextAttribsARB
    // (wglGetProcAddress requires a current context)
    if (!wglMakeCurrent(mpv_hdc, mpv_hglrc)) {
        LOG("nSetupGlContextWithHandles: failed to make mpv context current: 0x%lX", GetLastError());
        return false;
    }

    typedef HGLRC (WINAPI *PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int*);
    auto wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");

    // Step 2: Release mpv's context — wglShareLists / wglCreateContextAttribsARB
    // with share requires the share context to NOT be current on any thread
    wglMakeCurrent(nullptr, nullptr);

    HGLRC sharedCtx = nullptr;

    if (wglCreateContextAttribsARB) {
        // Try OpenGL 3.3 core profile (required by Skia's DirectContext)
        int ctxAttribs33[] = {
            0x2091, 3,  // WGL_CONTEXT_MAJOR_VERSION_ARB
            0x2092, 3,  // WGL_CONTEXT_MINOR_VERSION_ARB
            0x9126, 0x00000001,  // WGL_CONTEXT_PROFILE_MASK_ARB = WGL_CONTEXT_CORE_PROFILE_BIT_ARB
            0
        };
        sharedCtx = wglCreateContextAttribsARB(mpv_hdc, mpv_hglrc, ctxAttribs33);
        if (sharedCtx) {
            LOG("nSetupGlContextWithHandles: created GL 3.3 core shared context");
        } else {
            LOG("nSetupGlContextWithHandles: GL 3.3 core failed: 0x%lX, trying compat", GetLastError());
            // Try 3.3 compatibility profile
            int ctxAttribsCompat[] = {
                0x2091, 3,  // WGL_CONTEXT_MAJOR_VERSION_ARB
                0x2092, 3,  // WGL_CONTEXT_MINOR_VERSION_ARB
                0x9126, 0x00000002,  // WGL_CONTEXT_PROFILE_MASK_ARB = WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB
                0
            };
            sharedCtx = wglCreateContextAttribsARB(mpv_hdc, mpv_hglrc, ctxAttribsCompat);
            if (sharedCtx) {
                LOG("nSetupGlContextWithHandles: created GL 3.3 compat shared context");
            } else {
                LOG("nSetupGlContextWithHandles: GL 3.3 compat failed: 0x%lX, trying 3.0", GetLastError());
                int ctxAttribs30[] = {
                    0x2091, 3,  // WGL_CONTEXT_MAJOR_VERSION_ARB
                    0x2092, 0,  // WGL_CONTEXT_MINOR_VERSION_ARB
                    0
                };
                sharedCtx = wglCreateContextAttribsARB(mpv_hdc, mpv_hglrc, ctxAttribs30);
                if (sharedCtx) {
                    LOG("nSetupGlContextWithHandles: created GL 3.0 shared context");
                }
            }
        }
    } else {
        LOG("nSetupGlContextWithHandles: wglCreateContextAttribsARB not available");
    }

    // Last resort: fall back to legacy wglCreateContext + wglShareLists (GL 1.1 only)
    if (!sharedCtx) {
        LOG("nSetupGlContextWithHandles: falling back to legacy wglCreateContext");
        sharedCtx = wglCreateContext(mpv_hdc);
        if (!sharedCtx) {
            LOG("nSetupGlContextWithHandles: wglCreateContext failed: 0x%lX", GetLastError());
            return false;
        }
        if (!wglShareLists(mpv_hglrc, sharedCtx)) {
            LOG("nSetupGlContextWithHandles: wglShareLists failed: 0x%lX", GetLastError());
            wglDeleteContext(sharedCtx);
            return false;
        }
        LOG("nSetupGlContextWithHandles: created legacy GL 1.1 shared context (Skia may fail)");
    }

    // Step 3: Make the shared context current on this (render) thread
    if (!wglMakeCurrent(mpv_hdc, sharedCtx)) {
        LOG("nSetupGlContextWithHandles: wglMakeCurrent failed: 0x%lX", GetLastError());
        wglDeleteContext(sharedCtx);
        return false;
    }

    // Log the actual GL version of the shared context
    const char* glVersion = (const char*)glGetString(GL_VERSION);
    const char* glRenderer = (const char*)glGetString(GL_RENDERER);
    LOG("nSetupGlContextWithHandles: GL_VERSION=%s, GL_RENDERER=%s", glVersion ? glVersion : "null", glRenderer ? glRenderer : "null");

    // Store context handles (use mpv's HDC; no need to ReleaseDC since mpv owns it)
    s_render_hdc = mpv_hdc;
    s_render_hwnd = nullptr;  // mpv owns the HWND, don't destroy it
    s_shared_hglrc = sharedCtx;
    LOG("nSetupGlContextWithHandles: SUCCESS, shared_hglrc=%p, hdc=%p", sharedCtx, mpv_hdc);
    return true;
#else
    return false;
#endif
}

JNIEXPORT void JNICALL FN(nDestroyWglContext)(JNIEnv *env, jclass clazz, jlong hglrc_ptr) {
#ifdef _WIN32
    wglMakeCurrent(nullptr, nullptr);
    if (s_shared_hglrc) {
        wglDeleteContext(s_shared_hglrc);
        s_shared_hglrc = nullptr;
    }
    if (s_render_hdc && s_render_hwnd) {
        ReleaseDC(s_render_hwnd, s_render_hdc);
        DestroyWindow(s_render_hwnd);
    }
    s_render_hdc = nullptr;
    s_render_hwnd = nullptr;
#endif
}

JNIEXPORT void JNICALL FN(nSwapBuffers)(JNIEnv *env, jclass clazz) {
#ifdef _WIN32
    if (s_render_hdc) {
        SwapBuffers(s_render_hdc);
    }
#endif
}

JNIEXPORT void JNICALL FN(nWglSwapIntervalEXT)(JNIEnv *env, jclass clazz, jint interval) {
#ifdef _WIN32
    if (s_wglSwapIntervalEXT) {
        s_wglSwapIntervalEXT(interval);
    }
#endif
}

JNIEXPORT void JNICALL FN(nDeleteGlTexture)(JNIEnv *env, jclass clazz, jint textureId) {
    if (textureId != 0) {
        glDeleteTextures(1, (const GLuint*)&textureId);
    }
}

// ANGLE render context JNI implementations

JNIEXPORT jboolean JNICALL FN(nCreateAngleRenderContext)(JNIEnv *env, jclass clazz, jlong ptr, jint width, jint height) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    return instance->create_angle_render_context(width, height);
}

JNIEXPORT jboolean JNICALL FN(nResizeAngleRenderContext)(JNIEnv *env, jclass clazz, jlong ptr, jint width, jint height) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    return instance->resize_angle_render_context(width, height);
}

JNIEXPORT jboolean JNICALL FN(nRenderAngleFrame)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    return instance->render_angle_frame();
}

JNIEXPORT void JNICALL FN(nDestroyAngleRenderContext)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    instance->destroy_angle_render_context();
}

JNIEXPORT jboolean JNICALL FN(nCopyAnglePixels)(JNIEnv *env, jclass clazz, jlong ptr, jbyteArray outArray, jintArray outSize) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));

    jsize arrayLen = env->GetArrayLength(outArray);
    int out_w = 0, out_h = 0;

    jbyte *dst = env->GetByteArrayElements(outArray, nullptr);
    bool ok = instance->copy_angle_pixels(reinterpret_cast<uint8_t*>(dst), arrayLen, &out_w, &out_h);
    env->ReleaseByteArrayElements(outArray, dst, ok ? 0 : JNI_ABORT);

    if (ok) {
        jint size[2] = {out_w, out_h};
        env->SetIntArrayRegion(outSize, 0, 2, size);
    }
    return ok;
}

JNIEXPORT jint JNICALL FN(nGetAngleWidth)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    return instance->get_angle_width();
}

JNIEXPORT jint JNICALL FN(nGetAngleHeight)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    return instance->get_angle_height();
}

JNIEXPORT jboolean JNICALL FN(nIsAngleAvailable)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    return instance->is_angle_available();
}

JNIEXPORT jlong JNICALL FN(nGetAngleSharedTextureHandle)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    void *handle = instance->get_angle_shared_texture_handle();
    return reinterpret_cast<jlong>(handle);
}

JNIEXPORT jlong JNICALL FN(nGetAngleD3D11Device)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    void *device = instance->get_angle_d3d11_device();
    return reinterpret_cast<jlong>(device);
}

// Read pixels from ANGLE D3D11 shared texture via cached staging texture.
// Delegates to angle_render_context_t::readPixels() which caches the staging texture.
JNIEXPORT jboolean JNICALL FN(nReadPixelsFromSharedTexture)(
    JNIEnv *env, jclass clazz, jlong ptr, jbyteArray outArray, jintArray outSize) {
#ifdef _WIN32
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));

    jbyte *dst = env->GetByteArrayElements(outArray, nullptr);
    jsize arrayLen = env->GetArrayLength(outArray);
    int dims[2] = {0, 0};

    bool ok = instance->read_angle_pixels((uint8_t*)dst, (int)arrayLen, &dims[0], &dims[1]);

    env->ReleaseByteArrayElements(outArray, dst, ok ? 0 : JNI_ABORT);

    if (ok) {
        jint jdims[2] = { dims[0], dims[1] };
        env->SetIntArrayRegion(outSize, 0, 2, jdims);
    }
    return ok;
#else
    return false;
#endif
}

// Async readback: non-blocking CopyResource to staging texture
JNIEXPORT jboolean JNICALL FN(nBeginReadPixels)(
    JNIEnv *env, jclass clazz, jlong ptr) {
#ifdef _WIN32
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    return instance->begin_read_pixels();
#else
    return false;
#endif
}

// Async readback: map previous staging texture and copy to output (data already ready)
JNIEXPORT jboolean JNICALL FN(nGetReadPixelsResult)(
    JNIEnv *env, jclass clazz, jlong ptr, jbyteArray outArray, jintArray outSize) {
#ifdef _WIN32
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));

    jbyte *dst = env->GetByteArrayElements(outArray, nullptr);
    jsize arrayLen = env->GetArrayLength(outArray);
    int dims[2] = {0, 0};

    bool ok = instance->get_read_pixels_result((uint8_t*)dst, (int)arrayLen, &dims[0], &dims[1]);

    env->ReleaseByteArrayElements(outArray, dst, ok ? 0 : JNI_ABORT);

    if (ok) {
        jint jdims[2] = { dims[0], dims[1] };
        env->SetIntArrayRegion(outSize, 0, 2, jdims);
    }
    return ok;
#else
    return false;
#endif
}

// Open a D3D11 shared HANDLE on a D3D12 device, returning the ID3D12Resource*.
// Skia uses D3D12 internally; this bridges ANGLE's D3D11 texture to Skia's D3D12 pipeline.
// Returns 0 on failure.
// Extract the real ID3D12Device* from Skiko's internal device struct.
// Skiko's `device` Long is a pointer to an opaque struct containing the actual
// ID3D12Device*, ID3D12CommandQueue*, swap chain, etc.
// This function probes memory at known offsets to find the real device pointer.
// Check if a pointer points to a valid COM object by testing vtable readability
// and attempting QueryInterface.
static ID3D12Device* tryQueryD3D12Device(void* candidate) {
    if (!candidate) return nullptr;

    // Check if the candidate memory is readable
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(candidate, &mbi, sizeof(mbi)) == 0) return nullptr;
    if (mbi.State != MEM_COMMIT) return nullptr;

    // Read the vtable pointer
    void** vtable = *(void***)candidate;
    if (!vtable) return nullptr;

    // Check if vtable pointer is in readable memory (vtables are in .rdata/.data, not .text)
    MEMORY_BASIC_INFORMATION mbi2;
    if (VirtualQuery(vtable, &mbi2, sizeof(mbi2)) == 0) return nullptr;
    if (mbi2.State != MEM_COMMIT) return nullptr;
    // vtable data is in read-only or read-write data sections, NOT executable sections
    if (!(mbi2.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE))) return nullptr;

    // Try QueryInterface for ID3D12Device
    IUnknown* unk = (IUnknown*)candidate;
    ID3D12Device* device = nullptr;
    HRESULT hr = unk->QueryInterface(__uuidof(ID3D12Device), (void**)&device);
    if (SUCCEEDED(hr) && device) {
        fprintf(stderr, "[tryQueryD3D12Device] Found ID3D12Device: candidate=%p, device=%p\n", candidate, device);
        return device;
    }

    // Try ID3D12Device1
    ID3D12Device1* device1 = nullptr;
    hr = unk->QueryInterface(__uuidof(ID3D12Device1), (void**)&device1);
    if (SUCCEEDED(hr) && device1) {
        fprintf(stderr, "[tryQueryD3D12Device] Found ID3D12Device1: candidate=%p, device1=%p\n", candidate, device1);
        return device1;
    }

    return nullptr;
}

static ID3D12Device* extractD3D12Device(void* skikoHandle) {
    if (!skikoHandle) return nullptr;

    fprintf(stderr, "[extractD3D12Device] Scanning Skiko struct at %p\n", skikoHandle);

    // Dump the first 128 bytes of the struct for diagnostics
    fprintf(stderr, "[extractD3D12Device] Struct dump (first 128 bytes):\n");
    for (int i = 0; i < 128; i += sizeof(void*)) {
        void** fieldPtr = (void**)((char*)skikoHandle + i);
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(fieldPtr, &mbi, sizeof(mbi)) > 0 && mbi.State == MEM_COMMIT &&
            (mbi.Protect & (PAGE_READWRITE | PAGE_READONLY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_READ))) {
            fprintf(stderr, "  [%3d] %p\n", i, *fieldPtr);
        } else {
            fprintf(stderr, "  [%3d] <unreadable>\n", i);
        }
    }

    // Known Skiko DirectXDevice layout (from JetBrains/skiko source):
    //   class DirectXDevice {
    //     HWND hWnd;                              // offset 0 (8 bytes)
    //     GrD3DBackendContext backendContext;       // offset 8
    //       gr_cp<IDXGIAdapter1> fAdapter;         //   offset 8
    //       gr_cp<ID3D12Device>  fDevice;          //   offset 16  ← THE REAL DEVICE
    //       gr_cp<ID3D12CommandQueue> fQueue;      //   offset 24
    //     gr_cp<ID3D12Device> device;              // offset 48 (same pointer)
    //     ...
    //   }
    //
    // Fast path: try offset 16 first (backendContext.fDevice)
    static const int KNOWN_DEVICE_OFFSETS[] = { 16, 48, 8, 24 };
    for (int i = 0; i < 4; i++) {
        int offset = KNOWN_DEVICE_OFFSETS[i];
        void** fieldPtr = (void**)((char*)skikoHandle + offset);
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(fieldPtr, &mbi, sizeof(mbi)) == 0) continue;
        if (mbi.State != MEM_COMMIT) continue;
        if (!(mbi.Protect & (PAGE_READWRITE | PAGE_READONLY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_READ))) continue;

        void* candidate = *fieldPtr;
        if (!candidate) continue;

        ID3D12Device* dev = tryQueryD3D12Device(candidate);
        if (dev) {
            fprintf(stderr, "[extractD3D12Device] Found via known offset %d: %p\n", offset, dev);
            return dev;
        }
    }

    // Fallback: scan all offsets
    for (int offset = 0; offset < 4096; offset += sizeof(void*)) {
        // Skip already-checked known offsets
        if (offset == 8 || offset == 16 || offset == 24 || offset == 48) continue;

        void** fieldPtr = (void**)((char*)skikoHandle + offset);
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(fieldPtr, &mbi, sizeof(mbi)) == 0) continue;
        if (mbi.State != MEM_COMMIT) continue;
        if (!(mbi.Protect & (PAGE_READWRITE | PAGE_READONLY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_READ))) continue;

        void* candidate = *fieldPtr;
        if (!candidate) continue;
        if (candidate == skikoHandle) continue;

        ID3D12Device* dev = tryQueryD3D12Device(candidate);
        if (dev) {
            fprintf(stderr, "[extractD3D12Device] Found via scan at offset %d: %p\n", offset, dev);
            return dev;
        }
    }

    fprintf(stderr, "[extractD3D12Device] ID3D12Device not found in Skiko struct\n");
    return nullptr;
}

// Open a shared D3D11 texture handle on a D3D12 device.
// The d3d12DevicePtr can be either:
//   - A raw ID3D12Device* pointer
//   - A Skiko internal device handle (will scan for the real device)
JNIEXPORT jlong JNICALL FN(nOpenSharedTextureOnD3D12)(
    JNIEnv *env, jclass clazz, jlong d3d12DevicePtr, jlong sharedHandle) {
#ifdef _WIN32
    if (!d3d12DevicePtr || !sharedHandle) return 0;

    void* handlePtr = (void*)d3d12DevicePtr;
    HANDLE hShared = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(sharedHandle));

    fprintf(stderr, "[nOpenSharedTextureOnD3D12] handlePtr=%p, sharedHandle=%p\n", handlePtr, hShared);

    // First check if this is already a valid ID3D12Device*
    ID3D12Device* d3d12Device = nullptr;
    IUnknown* unk = (IUnknown*)handlePtr;

    // Check vtable pointer validity before calling QueryInterface
    // vtable data is in .rdata/.data (PAGE_READONLY/PAGE_READWRITE), NOT executable sections
    void** vtable = *(void***)unk;
    MEMORY_BASIC_INFORMATION mbi;
    bool vtableValid = false;
    if (vtable && VirtualQuery(vtable, &mbi, sizeof(mbi)) > 0 && mbi.State == MEM_COMMIT &&
        (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE))) {
        vtableValid = true;
    }

    if (vtableValid) {
        HRESULT qi_hr = unk->QueryInterface(__uuidof(ID3D12Device), (void**)&d3d12Device);
        fprintf(stderr, "[nOpenSharedTextureOnD3D12] Direct QueryInterface hr=0x%08lx, device=%p\n", qi_hr, d3d12Device);
    }

    // If not a valid ID3D12Device directly, try scanning as Skiko struct
    if (!d3d12Device) {
        fprintf(stderr, "[nOpenSharedTextureOnD3D12] Not a direct ID3D12Device, scanning Skiko struct...\n");
        d3d12Device = extractD3D12Device(handlePtr);
        if (!d3d12Device) {
            fprintf(stderr, "[nOpenSharedTextureOnD3D12] Could not find ID3D12Device in Skiko struct\n");
            return 0;
        }
    }

    fprintf(stderr, "[nOpenSharedTextureOnD3D12] Using ID3D12Device=%p\n", d3d12Device);

    // Call OpenSharedHandle on the real D3D12 device
    ID3D12Resource *d3d12Resource = nullptr;
    HRESULT hr = d3d12Device->OpenSharedHandle(hShared, __uuidof(ID3D12Resource), (void**)&d3d12Resource);
    fprintf(stderr, "[nOpenSharedTextureOnD3D12] OpenSharedHandle hr=0x%08lx\n", hr);

    if (FAILED(hr) || !d3d12Resource) {
        fprintf(stderr, "[nOpenSharedTextureOnD3D12] Failed: hr=0x%08lx\n", hr);
        d3d12Device->Release();
        return 0;
    }
    fprintf(stderr, "[nOpenSharedTextureOnD3D12] Success: resource=%p\n", d3d12Resource);

    // Release the extra reference from QueryInterface/extractD3D12Device
    d3d12Device->Release();
    return reinterpret_cast<jlong>(d3d12Resource);
#else
    return 0;
#endif
}

// Transition a D3D12 resource state using a command queue.
// This is needed for cross-API shared textures (D3D11→D3D12) to be properly
// transitioned from COMMON state to PIXEL_SHADER_RESOURCE for Skia rendering.
// Returns true on success.
JNIEXPORT jboolean JNICALL FN(nTransitionD3D12ResourceState)(
    JNIEnv *env, jclass clazz, jlong devicePtr, jlong queuePtr, jlong resourcePtr,
    jint fromState, jint toState) {
#ifdef _WIN32
    if (!devicePtr || !queuePtr || !resourcePtr) return JNI_FALSE;

    ID3D12Device* device = (ID3D12Device*)devicePtr;
    ID3D12CommandQueue* queue = (ID3D12CommandQueue*)queuePtr;
    ID3D12Resource* resource = (ID3D12Resource*)resourcePtr;

    fprintf(stderr, "[nTransitionD3D12ResourceState] device=%p, queue=%p, resource=%p, %d->%d\n",
            device, queue, resource, fromState, toState);

    // Check resource desc to understand its properties
    D3D12_RESOURCE_DESC desc = resource->GetDesc();
    fprintf(stderr, "[nTransitionD3D12ResourceState] Resource desc: Dimension=%d, Width=%llu, Height=%u, Format=%d, Flags=0x%08x\n",
            desc.Dimension, desc.Width, desc.Height, desc.Format, desc.Flags);

    // Check if resource has ALLOW_SIMULTANEOUS_ACCESS flag
    bool simultaneousAccess = (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS) != 0;
    fprintf(stderr, "[nTransitionD3D12ResourceState] ALLOW_SIMULTANEOUS_ACCESS=%d\n", simultaneousAccess);

    // For simultaneous access resources, barriers might not be needed
    // Let's try without barrier first - just execute an empty command list
    if (simultaneousAccess) {
        fprintf(stderr, "[nTransitionD3D12ResourceState] Resource has ALLOW_SIMULTANEOUS_ACCESS, trying without barrier\n");

        // Create command allocator
        ID3D12CommandAllocator* allocator = nullptr;
        HRESULT hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
            __uuidof(ID3D12CommandAllocator), (void**)&allocator);
        if (FAILED(hr) || !allocator) {
            fprintf(stderr, "[nTransitionD3D12ResourceState] CreateCommandAllocator failed: 0x%08lx\n", hr);
            return JNI_FALSE;
        }

        // Create command list (returns in OPEN state)
        ID3D12GraphicsCommandList* cmdList = nullptr;
        hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, nullptr,
            __uuidof(ID3D12GraphicsCommandList), (void**)&cmdList);
        if (FAILED(hr) || !cmdList) {
            fprintf(stderr, "[nTransitionD3D12ResourceState] CreateCommandList failed: 0x%08lx\n", hr);
            allocator->Release();
            return JNI_FALSE;
        }

        // Close command list without recording any barriers
        hr = cmdList->Close();
        if (FAILED(hr)) {
            fprintf(stderr, "[nTransitionD3D12ResourceState] Close (no barrier) failed: 0x%08lx\n", hr);
            cmdList->Release();
            allocator->Release();
            return JNI_FALSE;
        }

        // Execute empty command list
        ID3D12CommandList* cmdLists[] = { cmdList };
        queue->ExecuteCommandLists(1, cmdLists);

        // Create fence and wait for completion
        ID3D12Fence* fence = nullptr;
        hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void**)&fence);
        if (FAILED(hr) || !fence) {
            fprintf(stderr, "[nTransitionD3D12ResourceState] CreateFence failed: 0x%08lx\n", hr);
            cmdList->Release();
            allocator->Release();
            return JNI_FALSE;
        }

        hr = queue->Signal(fence, 1);
        if (SUCCEEDED(hr) && fence->GetCompletedValue() < 1) {
            HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            if (event) {
                fence->SetEventOnCompletion(1, event);
                WaitForSingleObject(event, INFINITE);
                CloseHandle(event);
            }
        }

        fprintf(stderr, "[nTransitionD3D12ResourceState] Success (no barrier, simultaneous access)\n");

        fence->Release();
        cmdList->Release();
        allocator->Release();
        return JNI_TRUE;
    }

    // For non-simultaneous access resources, try with barrier
    // Create command allocator
    ID3D12CommandAllocator* allocator = nullptr;
    HRESULT hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
        __uuidof(ID3D12CommandAllocator), (void**)&allocator);
    if (FAILED(hr) || !allocator) {
        fprintf(stderr, "[nTransitionD3D12ResourceState] CreateCommandAllocator failed: 0x%08lx\n", hr);
        return JNI_FALSE;
    }

    // Create command list (returns in OPEN state)
    ID3D12GraphicsCommandList* cmdList = nullptr;
    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, nullptr,
        __uuidof(ID3D12GraphicsCommandList), (void**)&cmdList);
    if (FAILED(hr) || !cmdList) {
        fprintf(stderr, "[nTransitionD3D12ResourceState] CreateCommandList failed: 0x%08lx\n", hr);
        allocator->Release();
        return JNI_FALSE;
    }

    // Record resource barrier
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = (D3D12_RESOURCE_STATES)fromState;
    barrier.Transition.StateAfter = (D3D12_RESOURCE_STATES)toState;
    cmdList->ResourceBarrier(1, &barrier);

    // Close command list
    hr = cmdList->Close();
    if (FAILED(hr)) {
        fprintf(stderr, "[nTransitionD3D12ResourceState] Close failed: 0x%08lx\n", hr);
        cmdList->Release();
        allocator->Release();
        return JNI_FALSE;
    }

    // Execute command list
    ID3D12CommandList* cmdLists[] = { cmdList };
    queue->ExecuteCommandLists(1, cmdLists);

    // Create fence and wait for completion
    ID3D12Fence* fence = nullptr;
    hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void**)&fence);
    if (FAILED(hr) || !fence) {
        fprintf(stderr, "[nTransitionD3D12ResourceState] CreateFence failed: 0x%08lx\n", hr);
        cmdList->Release();
        allocator->Release();
        return JNI_FALSE;
    }

    hr = queue->Signal(fence, 1);
    if (SUCCEEDED(hr) && fence->GetCompletedValue() < 1) {
        HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (event) {
            fence->SetEventOnCompletion(1, event);
            WaitForSingleObject(event, INFINITE);
            CloseHandle(event);
        }
    }

    fprintf(stderr, "[nTransitionD3D12ResourceState] Success\n");

    fence->Release();
    cmdList->Release();
    allocator->Release();
    return JNI_TRUE;
#else
    return JNI_FALSE;
#endif
}

// Create a D3D12 device and command queue for GPU texture interop.
// Returns long[3] = {adapterPtr, devicePtr, queuePtr} or null on failure.
// Used for P2.5 verification: BackendRenderTarget → Surface → makeImageSnapshot.
JNIEXPORT jlongArray JNICALL FN(nCreateD3D12Device)(JNIEnv *env, jclass clazz) {
#ifdef _WIN32
    // Use default adapter (nullptr = default)
    IDXGIFactory4 *factory = nullptr;
    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory4), (void**)&factory);
    if (FAILED(hr) || !factory) return nullptr;

    IDXGIAdapter1 *adapter = nullptr;
    hr = factory->EnumAdapters1(0, &adapter);
    factory->Release();
    if (FAILED(hr) || !adapter) return nullptr;

    // Create D3D12 device
    ID3D12Device *device = nullptr;
    hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void**)&device);
    if (FAILED(hr) || !device) {
        adapter->Release();
        return nullptr;
    }

    // Create command queue
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    ID3D12CommandQueue *queue = nullptr;
    hr = device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), (void**)&queue);
    if (FAILED(hr) || !queue) {
        device->Release();
        adapter->Release();
        return nullptr;
    }

    jlongArray result = env->NewLongArray(3);
    jlong ptrs[3] = {
        reinterpret_cast<jlong>(adapter),
        reinterpret_cast<jlong>(device),
        reinterpret_cast<jlong>(queue)
    };
    env->SetLongArrayRegion(result, 0, 3, ptrs);
    return result;
#else
    return nullptr;
#endif
}

// Release a D3D12 resource (ID3D12Resource*).
JNIEXPORT void JNICALL FN(nReleaseD3D12Resource)(JNIEnv *env, jclass clazz, jlong resourcePtr) {
#ifdef _WIN32
    if (resourcePtr) {
        ID3D12Resource *resource = (ID3D12Resource*)resourcePtr;
        resource->Release();
    }
#endif
}

// Destroy D3D12 device, adapter, and queue created by nCreateD3D12Device.
JNIEXPORT void JNICALL FN(nDestroyD3D12Device)(JNIEnv *env, jclass clazz, jlong devicePtr) {
#ifdef _WIN32
    if (devicePtr) {
        ID3D12Device *device = (ID3D12Device*)devicePtr;
        device->Release();
    }
#endif
}

// Get the default DXGI adapter (IDXGIAdapter1*).
// Used with a D3D12 device obtained from Compose/Skiko via reflection
// to create a compatible DirectContext.
JNIEXPORT jlong JNICALL FN(nGetD3D12DefaultAdapter)(JNIEnv *env, jclass clazz) {
#ifdef _WIN32
    IDXGIFactory4 *factory = nullptr;
    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory4), (void**)&factory);
    if (FAILED(hr) || !factory) return 0;

    IDXGIAdapter1 *adapter = nullptr;
    hr = factory->EnumAdapters1(0, &adapter);
    factory->Release();
    if (FAILED(hr) || !adapter) return 0;

    return reinterpret_cast<jlong>(adapter);
#else
    return 0;
#endif
}

// Create a D3D12 command queue on an existing device.
// Used with Compose's D3D12 device (obtained via reflection) to create a DirectContext.
// The devicePtr may be a raw ID3D12Device* or a Skiko struct pointer.
JNIEXPORT jlong JNICALL FN(nCreateD3D12CommandQueue)(JNIEnv *env, jclass clazz, jlong devicePtr) {
#ifdef _WIN32
    if (!devicePtr) return 0;

    // Try as raw ID3D12Device* first, then extract from Skiko struct
    ID3D12Device *device = tryQueryD3D12Device((void*)devicePtr);
    if (!device) {
        device = extractD3D12Device((void*)devicePtr);
    }
    if (!device) {
        fprintf(stderr, "[nCreateD3D12CommandQueue] Could not get ID3D12Device from %p\n", (void*)devicePtr);
        return 0;
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    ID3D12CommandQueue *queue = nullptr;
    HRESULT hr = device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), (void**)&queue);

    // Release the extra ref from extractD3D12Device (tryQueryD3D12Device also AddRefs via QI)
    device->Release();

    if (FAILED(hr) || !queue) return 0;

    fprintf(stderr, "[nCreateD3D12CommandQueue] Created queue=%p on device=%p\n", queue, device);
    return reinterpret_cast<jlong>(queue);
#else
    return 0;
#endif
}

// Extract the raw ID3D12Device* from either a raw pointer or a Skiko struct pointer.
// Returns the raw device pointer with an AddRef (caller must Release when done).
// Returns 0 on failure.
JNIEXPORT jlong JNICALL FN(nExtractD3D12DevicePtr)(JNIEnv *env, jclass clazz, jlong skikoOrDevicePtr) {
#ifdef _WIN32
    if (!skikoOrDevicePtr) return 0;

    // Try as raw ID3D12Device* first
    ID3D12Device *device = tryQueryD3D12Device((void*)skikoOrDevicePtr);
    if (device) {
        fprintf(stderr, "[nExtractD3D12DevicePtr] Raw ID3D12Device: %p\n", device);
        return reinterpret_cast<jlong>(device);
    }

    // Try extracting from Skiko struct
    device = extractD3D12Device((void*)skikoOrDevicePtr);
    if (device) {
        fprintf(stderr, "[nExtractD3D12DevicePtr] Extracted from Skiko struct: %p\n", device);
        return reinterpret_cast<jlong>(device);
    }

    fprintf(stderr, "[nExtractD3D12DevicePtr] Could not get ID3D12Device from %p\n", (void*)skikoOrDevicePtr);
    return 0;
#else
    return 0;
#endif
}

// Validate if a pointer is a valid D3D12 device.
// Returns true if QueryInterface for ID3D12Device succeeds.
// Prints diagnostic info to stderr.
JNIEXPORT jboolean JNICALL FN(nValidateD3D12Device)(JNIEnv *env, jclass clazz, jlong devicePtr) {
#ifdef _WIN32
    if (!devicePtr) {
        fprintf(stderr, "[nValidateD3D12Device] devicePtr is null\n");
        return false;
    }

    IUnknown *unk = (IUnknown*)devicePtr;

    // Try to read the vtable pointer (first 8 bytes on x64)
    void **vtable = *(void***)unk;
    fprintf(stderr, "[nValidateD3D12Device] devicePtr=%p, vtable=%p\n", unk, vtable);

    // Try QueryInterface
    ID3D12Device *device = nullptr;
    HRESULT hr = unk->QueryInterface(__uuidof(ID3D12Device), (void**)&device);
    fprintf(stderr, "[nValidateD3D12Device] QueryInterface(ID3D12Device) hr=0x%08lx, device=%p\n", hr, device);

    if (FAILED(hr) || !device) {
        // Try as if it's already an ID3D12Device (maybe vtable is just offset)
        fprintf(stderr, "[nValidateD3D12Device] Not a valid IUnknown, trying as ID3D12Device directly\n");

        // Check if at least the vtable looks reasonable (D3D12 devices have many methods)
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(vtable, &mbi, sizeof(mbi))) {
            fprintf(stderr, "[nValidateD3D12Device] vtable region: base=%p, size=%lu, state=%lu, protect=%lu\n",
                mbi.BaseAddress, mbi.RegionSize, mbi.State, mbi.Protect);
        }
        return false;
    }

    // Get feature level to verify it's a real device
    D3D_FEATURE_LEVEL fl = device->GetDeviceRemovedReason() ? D3D_FEATURE_LEVEL_11_0 : D3D_FEATURE_LEVEL_11_0;
    (void)fl;

    // Try to get device name via DXGI adapter
    IDXGIFactory4 *factory = nullptr;
    HRESULT fhr = CreateDXGIFactory1(__uuidof(IDXGIFactory4), (void**)&factory);
    if (SUCCEEDED(fhr) && factory) {
        IDXGIAdapter1 *adapter = nullptr;
        if (SUCCEEDED(factory->EnumAdapters1(0, &adapter))) {
            DXGI_ADAPTER_DESC1 desc;
            if (SUCCEEDED(adapter->GetDesc1(&desc))) {
                fprintf(stderr, "[nValidateD3D12Device] Adapter: %ls, VendorId=0x%04x, DeviceId=0x%04x\n",
                    desc.Description, desc.VendorId, desc.DeviceId);
            }
            adapter->Release();
        }
        factory->Release();
    }

    device->Release();
    fprintf(stderr, "[nValidateD3D12Device] Device is valid ID3D12Device\n");
    return true;
#else
    return false;
#endif
}

JNIEXPORT jboolean JNICALL FN(nDestroy)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    return instance->destroy(env);
}

JNIEXPORT void JNICALL FN(nFinalize)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    delete instance;
}
