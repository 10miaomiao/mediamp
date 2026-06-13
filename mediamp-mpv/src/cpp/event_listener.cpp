#include "mpv_handle_t.h"
#include "method_cache.h"

namespace mediampv {

static void emit_property_change(JNIEnv *env, mpv_event_property *prop, jobject *event_listener) {
    jstring prop_name = env->NewStringUTF(prop->name);
    jstring value = nullptr;
    
    switch (prop->format) {
        case MPV_FORMAT_NONE:
            env->CallVoidMethod(*event_listener,
                                jni_mediamp_method_EventListener_onPropertyChange_NONE,
                                prop_name);
            break;
        case MPV_FORMAT_FLAG:
            env->CallVoidMethod(*event_listener,
                                jni_mediamp_method_EventListener_onPropertyChange_FLAG,
                                prop_name,
                                prop->data ? (jboolean) (*(int *) prop->data != 0) : JNI_FALSE);
            break;
        case MPV_FORMAT_INT64: {
            jlong val = prop->data ? (jlong) *(int64_t *) prop->data : 0L;
            env->CallVoidMethod(*event_listener,
                                jni_mediamp_method_EventListener_onPropertyChange_INT64,
                                prop_name, val);
            if (env->ExceptionCheck()) {
                LOG("emit_property_change: Exception in INT64 callback for %s", prop->name);
                env->ExceptionDescribe();
                env->ExceptionClear();
            }
            break;
        }
        case MPV_FORMAT_DOUBLE:
            env->CallVoidMethod(*event_listener,
                                jni_mediamp_method_EventListener_onPropertyChange_DOUBLE,
                                prop_name,
                                prop->data ? (jdouble) *(double *) prop->data : 0.0);
            break;
        case MPV_FORMAT_STRING:
            if (prop->data && *(const char **) prop->data) {
                value = env->NewStringUTF(*(const char **) prop->data);
            }
            env->CallVoidMethod(*event_listener,
                                jni_mediamp_method_EventListener_onPropertyChange_STRING,
                                prop_name,
                                value);
            break;
        default:
            LOG("emit_property_change: Unknown property update format received in callback: %d", prop->format);
            break;
    }
    
    if (prop_name) env->DeleteLocalRef(prop_name);
    if (value) env->DeleteLocalRef(value);
}

void *(mpv_handle_t::event_loop)(void *arg) {
    if (!jvm_ || !handle_) {
        LOG("[event_loop] jvm or mpv handle_ is not initialized, event loop will not start");
        return nullptr;
    }

    JNIEnv *env = nullptr;
    int getEnvResult = jvm_->GetEnv((void **) &env, JNI_VERSION_1_6);
    if (getEnvResult == JNI_EDETACHED) {
#ifdef __ANDROID__
        if (jvm_->AttachCurrentThread(&env, nullptr) != JNI_OK) {
#else
        if (jvm_->AttachCurrentThread(reinterpret_cast<void **>(&env), nullptr) != JNI_OK) {
#endif // __ANDROID__
            LOG("[event_loop] failed to attach current thread");
            return nullptr;
        }
    } else if (getEnvResult != JNI_OK) {
        LOG("[event_loop] failed to get JNI env");
        return nullptr;
    }

    LOG("[event_loop] event loop is started.");
    while (!event_loop_request_exit) {
        if (!handle_) {
            LOG("[event_loop] mpv handle_ is destroyed, event loop will stop");
            break;
        }

        mpv_event *event;
        mpv_event_property *event_property = nullptr;
        mpv_event_log_message *log_message = nullptr;

        // 不处理 NONE 事件
        if ((event = mpv_wait_event(handle_, -1.0))->event_id == MPV_EVENT_NONE) {
            continue;
        }

        switch (event->event_id) {
            case MPV_EVENT_PROPERTY_CHANGE:
                event_property = (mpv_event_property *) event->data;
                emit_property_change(env, event_property, event_listener_);
                LOG("[event_loop] property change: %s", event_property->name);
                break;
            case MPV_EVENT_LOG_MESSAGE:
                log_message = (mpv_event_log_message *) event->data;
                LOG("[event_loop] [%s:%s] %s", log_message->prefix, log_message->level, log_message->text);
                break;
            case MPV_EVENT_IDLE:
                LOG("[event_loop] idle");
                break;
            default:
                LOG("[event_loop] unhandled event: %d", event->event_id);
                break;
        }

    }
    LOG("[event_loop] event loop is stopped.");

    if (jvm_) jvm_->DetachCurrentThread();
    return nullptr;
}
    
} // namespace mediampv