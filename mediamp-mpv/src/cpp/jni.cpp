#include <iostream>
#include <jni.h>
#include <cstdio>
#include "include/method_cache.h"
#include "include/mpv_handle_t.h"

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
    JNIEXPORT jboolean JNICALL FN(nRenderSwFrame)(JNIEnv *env, jclass clazz, jlong ptr);
    JNIEXPORT void JNICALL FN(nDestroySwRenderContext)(JNIEnv *env, jclass clazz, jlong ptr);
    JNIEXPORT jboolean JNICALL FN(nCopySwPixels)(JNIEnv *env, jclass clazz, jlong ptr, jbyteArray outArray);
    JNIEXPORT jint JNICALL FN(nGetSwWidth)(JNIEnv *env, jclass clazz, jlong ptr);
    JNIEXPORT jint JNICALL FN(nGetSwHeight)(JNIEnv *env, jclass clazz, jlong ptr);

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
    int result = 0;

    instance->get_property(key_char, MPV_FORMAT_INT64, &result);
    env->ReleaseStringUTFChars(key, key_char);

    return result;
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

JNIEXPORT jboolean JNICALL FN(nRenderSwFrame)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    return instance->render_sw_frame();
}

JNIEXPORT void JNICALL FN(nDestroySwRenderContext)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    instance->destroy_sw_render_context();
}

JNIEXPORT jboolean JNICALL FN(nCopySwPixels)(JNIEnv *env, jclass clazz, jlong ptr, jbyteArray outArray) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    const uint8_t *pixels = instance->get_sw_pixels();
    int width = instance->get_sw_width();
    int height = instance->get_sw_height();

    if (!pixels || width <= 0 || height <= 0) return false;

    jsize size = width * height * 4;
    jsize arrayLen = env->GetArrayLength(outArray);
    if (arrayLen < size) return false;

    env->SetByteArrayRegion(outArray, 0, size, reinterpret_cast<const jbyte*>(pixels));
    return true;
}

JNIEXPORT jint JNICALL FN(nGetSwWidth)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    return instance->get_sw_width();
}

JNIEXPORT jint JNICALL FN(nGetSwHeight)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    return instance->get_sw_height();
}

JNIEXPORT jboolean JNICALL FN(nDestroy)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    return instance->destroy(env);
}

JNIEXPORT void JNICALL FN(nFinalize)(JNIEnv *env, jclass clazz, jlong ptr) {
    auto* instance = reinterpret_cast<mediampv::mpv_handle_t *>(static_cast<uintptr_t>(ptr));
    delete instance;
}
