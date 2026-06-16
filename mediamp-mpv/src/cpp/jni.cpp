#include <iostream>
#include <jni.h>
#include <cstdio>
#include "include/method_cache.h"
#include "include/mpv_handle_t.h"

#ifdef _WIN32
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi.h>
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
    JNIEXPORT jlong JNICALL FN(nOpenSharedTextureOnD3D12)(JNIEnv *env, jclass clazz, jlong d3d12DevicePtr, jlong sharedHandle);

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

// Open a D3D11 shared HANDLE on a D3D12 device, returning the ID3D12Resource*.
// Skia uses D3D12 internally; this bridges ANGLE's D3D11 texture to Skia's D3D12 pipeline.
// Returns 0 on failure.
JNIEXPORT jlong JNICALL FN(nOpenSharedTextureOnD3D12)(
    JNIEnv *env, jclass clazz, jlong d3d12DevicePtr, jlong sharedHandle) {
#ifdef _WIN32
    if (!d3d12DevicePtr || !sharedHandle) return 0;

    ID3D12Device *d3d12Device = (ID3D12Device*)d3d12DevicePtr;
    HANDLE hShared = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(sharedHandle));

    ID3D12Resource *d3d12Resource = nullptr;
    HRESULT hr = d3d12Device->OpenSharedHandle(hShared, __uuidof(ID3D12Resource), (void**)&d3d12Resource);
    if (FAILED(hr) || !d3d12Resource) {
        return 0;
    }
    return reinterpret_cast<jlong>(d3d12Resource);
#else
    return 0;
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
