#include <iostream>
#include "include/mpv_handle_t.h"
#include "include/method_cache.h"
#include "include/compatible_thread.h"
#include "include/global_lock.h"
#include <mpv/render.h>

#if defined(__ANDROID__)
extern "C" {
#include <libavcodec/jni.h>
}
#endif

#define CHECK_HANDLE() if (!handle_) { \
    LOG("mpv handle is not created when %s", __FUNCTION__); \
    return false; \
}

namespace mediampv {

CREATE_LOCK(global_guard);
JavaVM *global_jvm = nullptr;

void mpv_handle_t::create(JNIEnv *env, jobject app_context) {
    FP;
    LOCK(global_guard);

    if (!global_jvm) {
        env->GetJavaVM(&global_jvm);
        if (!global_jvm) {
            LOG("failed to get current jvm");
            exit(1); // TODO: don't exit
        }

#if defined(__ANDROID__)
        av_jni_set_java_vm(global_jvm, &app_context);
#endif
    }

    jvm_ = global_jvm;
    handle_ = mpv_create();

    // use terminal log level but request verbose messages
    // this way --msg-level can be used to adjust later
    mpv_request_log_messages(handle_, "terminal-default");
    mpv_set_option_string(handle_, "msg-level", "all=v");
}

bool mpv_handle_t::initialize() {
    FP;

    if (!handle_) return false;
    if (mpv_initialize(handle_) < 0) {
        LOG("failed to initialize mpv");
        return false;
    }

    event_thread_ = std::make_shared<mediampv::compatible_thread>([&] { event_loop(0); });
    if (!event_thread_->create()) {
        LOG("failed to create event thread");
        return false;
    }

    return true;
}

bool mpv_handle_t::set_event_listener(JNIEnv *env, jobject listener) {
    FP;

    if (event_listener_ && *event_listener_) {
        env->DeleteGlobalRef(*event_listener_);
        event_listener_ = nullptr;
    }
    mediampv::jni_cache_classes(env);

    if (env->IsInstanceOf(listener, mediampv::jni_mediamp_clazz_EventListener) != JNI_TRUE) {
        LOG("listener is not an instance of EventListener");
        return false;
    }

    if (!event_listener_) event_listener_ = new jobject;
    *event_listener_ = env->NewGlobalRef(listener);

    return true;
}

bool mpv_handle_t::command(const char **args) {
    FP;
    CHECK_HANDLE()
    return mpv_command(handle_, args) >= 0;
}

bool mpv_handle_t::set_option(const char *key, const char *value) {
    FP;
    CHECK_HANDLE()
    return mpv_set_option_string(handle_, key, value);
}

bool mpv_handle_t::get_property(const char *name, mpv_format format, void *out_result) {
    FP;
    CHECK_HANDLE()
    return mpv_get_property(handle_, name, format, out_result) >= 0;
}

bool mpv_handle_t::set_property(const char *name, mpv_format format, void *in_value) {
    FP;
    CHECK_HANDLE()
    return mpv_set_property(handle_, name, format, in_value) >= 0;
}

bool mpv_handle_t::observe_property(const char *property, mpv_format format, uint64_t reply_data) {
    FP;
    CHECK_HANDLE()
    return mpv_observe_property(handle_, reply_data, property, format) >= 0;
}

bool mpv_handle_t::unobserve_property(uint64_t reply_data) {
    FP;
    CHECK_HANDLE()
    return mpv_unobserve_property(handle_, reply_data) >= 0;
}

CREATE_LOCK(surface_access_lock);

bool mpv_handle_t::attach_android_surface(JNIEnv *env, jobject surface) {
    FP;
    LOCK(surface_access_lock);
    CHECK_HANDLE()

#ifdef __ANDROID__
    if (surface_attached_) detach_android_surface(env);
    if (env->IsInstanceOf(surface, mediampv::jni_mediamp_clazz_android_Surface) != JNI_TRUE) {
        LOG("surface is not instance of android.view.Surface");
        return false;
    }

    jobject ref = env->NewGlobalRef(surface);
    int64_t wid = (int64_t)(intptr_t) ref;
    surface_ = ref;
    surface_attached_ = mpv_set_option(handle_, "wid", MPV_FORMAT_INT64, &wid) >= 0;

    return surface_attached_;
#else
    LOG("attach_android_surface is only implemented on Android");
    return false;
#endif
}

bool mpv_handle_t::detach_android_surface(JNIEnv *env) {
    FP;
    LOCK(surface_access_lock);
    CHECK_HANDLE()

#ifdef __ANDROID__
    if (!surface_attached_) return false;

    int64_t wid = 0;
    bool result = mpv_set_option(handle_, "wid", MPV_FORMAT_INT64, (void*) &wid);
    env->DeleteGlobalRef(surface_);
    surface_attached_ = false;

    return result;
#else
    LOG("detach_android_surface is only implemented on Android");
    return false;
#endif
}

// Software render context (vo=libmpv)

bool mpv_handle_t::create_sw_render_context(int width, int height) {
    FP;
    CHECK_HANDLE()

    if (sw_render_ctx_) {
        destroy_sw_render_context();
    }

    sw_width_ = width;
    sw_height_ = height;
    sw_pixel_buffer_ = new uint8_t[width * height * 4];
    memset(sw_pixel_buffer_, 0, width * height * 4);

    int mpv_advanced = 1;
    const char *api_type = MPV_RENDER_API_TYPE_SW;
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(api_type)},
        {MPV_RENDER_PARAM_ADVANCED_CONTROL, &mpv_advanced},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };

    int result = mpv_render_context_create(&sw_render_ctx_, handle_, params);
    if (result < 0) {
        LOG("Failed to create SW mpv render context: %d", result);
        delete[] sw_pixel_buffer_;
        sw_pixel_buffer_ = nullptr;
        sw_render_ctx_ = nullptr;
        return false;
    }

    // Register update callback for frame notifications
    mpv_render_context_set_update_callback(sw_render_ctx_, [](void *ctx) {
        // Frame is ready, the render thread will pick it up
    }, nullptr);

    LOG("SW render context created: %dx%d", width, height);
    return true;
}

bool mpv_handle_t::render_sw_frame() {
    if (!sw_render_ctx_ || !sw_pixel_buffer_) return false;

    int32_t size[] = {sw_width_, sw_height_};
    int pitch = 4 * sw_width_;

    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_SW_SIZE, size},
        {MPV_RENDER_PARAM_SW_FORMAT, const_cast<char*>("bgra")},
        {MPV_RENDER_PARAM_SW_STRIDE, &pitch},
        {MPV_RENDER_PARAM_SW_POINTER, sw_pixel_buffer_},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };

    int result = mpv_render_context_render(sw_render_ctx_, params);
    if (result < 0) {
        LOG("mpv_render_context_render (SW) failed: %d", result);
        return false;
    }

    // Debug: check first few pixels
    static int render_count = 0;
    render_count++;
    if (render_count <= 5 || render_count % 300 == 0) {
        bool has_content = false;
        int check_size = (sw_width_ * sw_height_ * 4 < 4000) ? sw_width_ * sw_height_ * 4 : 4000;
        for (int i = 0; i < check_size; i++) {
            if (sw_pixel_buffer_[i] != 0) { has_content = true; break; }
        }
        uint32_t first_pixel = *(uint32_t*)sw_pixel_buffer_;
        LOG("SW render #%d: result=%d, first_pixel=0x%08X, has_content=%d, size=%dx%d",
            render_count, result, first_pixel, has_content, sw_width_, sw_height_);
    }

    return true;
}

void mpv_handle_t::destroy_sw_render_context() {
    if (sw_render_ctx_) {
        mpv_render_context_free(sw_render_ctx_);
        sw_render_ctx_ = nullptr;
    }
    if (sw_pixel_buffer_) {
        delete[] sw_pixel_buffer_;
        sw_pixel_buffer_ = nullptr;
    }
    sw_width_ = 0;
    sw_height_ = 0;
}

const uint8_t *mpv_handle_t::get_sw_pixels() const {
    return sw_pixel_buffer_;
}

int mpv_handle_t::get_sw_width() const {
    return sw_width_;
}

int mpv_handle_t::get_sw_height() const {
    return sw_height_;
}

bool mpv_handle_t::destroy(JNIEnv *env) {
    FP;
    CHECK_HANDLE()

    // Destroy SW render context before mpv handle
    destroy_sw_render_context();

    event_loop_request_exit = true;
    mpv_wakeup(handle_);

    if (!event_thread_) {
        LOG("event thread is not created when destroy mpv handle");
        return false;
    }
    event_thread_->join();

    if (event_listener_) env->DeleteGlobalRef(*event_listener_);
    mpv_terminate_destroy(handle_);

    return true;
}

} // namespace mediampv
