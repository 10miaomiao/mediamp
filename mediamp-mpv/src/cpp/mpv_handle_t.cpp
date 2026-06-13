#include <iostream>
#include "include/mpv_handle_t.h"
#include "include/method_cache.h"
#include "include/compatible_thread.h"
#include "include/global_lock.h"
#include <thread>
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
    sw_front_buffer_ = new uint8_t[width * height * 4];
    memset(sw_pixel_buffer_, 0, width * height * 4);
    memset(sw_front_buffer_, 0, width * height * 4);

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
        delete[] sw_front_buffer_;
        sw_pixel_buffer_ = nullptr;
        sw_front_buffer_ = nullptr;
        sw_render_ctx_ = nullptr;
        return false;
    }

    // Register update callback for event-driven rendering
    mpv_render_context_set_update_callback(sw_render_ctx_, [](void *ctx) {
        auto *self = static_cast<mpv_handle_t*>(ctx);
        {
            std::lock_guard<std::mutex> lock(self->frame_mutex_);
            self->frame_ready_ = true;
        }
        self->frame_cv_.notify_one();
    }, this);

    LOG("SW render context created: %dx%d", width, height);
    return true;
}

bool mpv_handle_t::resize_sw_render_context(int width, int height) {
    if (!sw_render_ctx_) return false;
    if (width <= 0 || height <= 0) return false;
    if (width == sw_width_ && height == sw_height_) return true; // no change

    LOG("Resizing SW render context: %dx%d -> %dx%d", sw_width_, sw_height_, width, height);

    // Allocate new buffers
    auto *new_back = new(std::nothrow) uint8_t[width * height * 4];
    auto *new_front = new(std::nothrow) uint8_t[width * height * 4];
    if (!new_back || !new_front) {
        delete[] new_back;
        delete[] new_front;
        LOG("Failed to allocate buffers for resize");
        return false;
    }
    memset(new_back, 0, width * height * 4);
    memset(new_front, 0, width * height * 4);

    // Synchronize with render thread
    std::lock_guard<std::mutex> lock(resize_mutex_);
    delete[] sw_pixel_buffer_;
    delete[] sw_front_buffer_;
    sw_pixel_buffer_ = new_back;
    sw_front_buffer_ = new_front;
    sw_width_ = width;
    sw_height_ = height;

    return true;
}

bool mpv_handle_t::render_sw_frame() {
    if (!sw_render_ctx_ || !sw_pixel_buffer_) return false;

    std::lock_guard<std::mutex> lock(resize_mutex_);

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
        return false;
    }

    // Swap back and front buffers:
    // mpv wrote to sw_pixel_buffer_ (back), display reads from sw_front_buffer_
    std::swap(sw_pixel_buffer_, sw_front_buffer_);

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
    if (sw_front_buffer_) {
        delete[] sw_front_buffer_;
        sw_front_buffer_ = nullptr;
    }
    sw_width_ = 0;
    sw_height_ = 0;
    video_width_ = 0;
    video_height_ = 0;
    // Wake up any thread blocked in wait_for_frame()
    frame_cv_.notify_all();
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

int mpv_handle_t::get_video_width() const {
    return video_width_;
}

int mpv_handle_t::get_video_height() const {
    return video_height_;
}

bool mpv_handle_t::query_video_resolution(int *out_w, int *out_h) {
    if (!handle_) return false;

    // Try reading video-params as NODE first
    mpv_node node;
    int result = mpv_get_property(handle_, "video-params", MPV_FORMAT_NODE, &node);
    if (result >= 0 && node.format == MPV_FORMAT_NODE_MAP && node.u.list) {
        *out_w = 0;
        *out_h = 0;
        mpv_node_list *list = node.u.list;
        for (int i = 0; i < list->num; i++) {
            const char *key = list->keys[i];
            if (!key) continue;
            if (strcmp(key, "w") == 0 && list->values[i].format == MPV_FORMAT_INT64) {
                *out_w = (int) list->values[i].u.int64;
            } else if (strcmp(key, "h") == 0 && list->values[i].format == MPV_FORMAT_INT64) {
                *out_h = (int) list->values[i].u.int64;
            }
        }
        mpv_free_node_contents(&node);
        if (*out_w > 0 && *out_h > 0) {
            LOG("query_video_resolution (NODE): %dx%d", *out_w, *out_h);
            return true;
        }
    }

    // Fallback: try reading as INT64 sub-properties directly
    int64_t w = 0, h = 0;
    if (mpv_get_property(handle_, "video-params/w", MPV_FORMAT_INT64, &w) >= 0 &&
        mpv_get_property(handle_, "video-params/h", MPV_FORMAT_INT64, &h) >= 0) {
        *out_w = (int) w;
        *out_h = (int) h;
        if (*out_w > 0 && *out_h > 0) {
            LOG("query_video_resolution (INT64 sub-prop): %dx%d", *out_w, *out_h);
            return true;
        }
    }

    // Fallback: try width/height properties
    if (mpv_get_property(handle_, "width", MPV_FORMAT_INT64, &w) >= 0 &&
        mpv_get_property(handle_, "height", MPV_FORMAT_INT64, &h) >= 0) {
        *out_w = (int) w;
        *out_h = (int) h;
        if (*out_w > 0 && *out_h > 0) {
            LOG("query_video_resolution (width/height): %dx%d", *out_w, *out_h);
            return true;
        }
    }

    LOG("query_video_resolution: all methods failed");
    *out_w = 0;
    *out_h = 0;
    return false;
}

void mpv_handle_t::wait_for_frame() {
    // Hybrid: try condition variable first (fast path from callback),
    // fall back to polling mpv_render_context_update() if callback doesn't fire.
    while (sw_render_ctx_ || gl_render_ctx_ || angle_render_ctx_) {
        {
            std::unique_lock<std::mutex> lock(frame_mutex_);
            if (frame_cv_.wait_for(lock, std::chrono::milliseconds(2),
                [this] { return frame_ready_ || (!sw_render_ctx_ && !gl_render_ctx_ && !angle_render_ctx_); })) {
                frame_ready_ = false;
                return;
            }
        }
        // Callback didn't fire within 2ms, poll mpv directly
        mpv_render_context *ctx = sw_render_ctx_;
        if (!ctx && gl_render_ctx_) {
            ctx = gl_render_ctx_->getMpvRenderContext();
        }
        if (!ctx && angle_render_ctx_) {
            ctx = angle_render_ctx_->getMpvRenderContext();
        }
        if (ctx) {
            uint64_t flags = mpv_render_context_update(ctx);
            if (flags & MPV_RENDER_UPDATE_FRAME) {
                return;
            }
        }
    }
}

bool mpv_handle_t::has_pending_frame() {
    mpv_render_context *ctx = sw_render_ctx_;
    if (!ctx && gl_render_ctx_) {
        ctx = gl_render_ctx_->getMpvRenderContext();
    }
    if (!ctx && angle_render_ctx_) {
        ctx = angle_render_ctx_->getMpvRenderContext();
    }
    if (!ctx) return false;
    uint64_t flags = mpv_render_context_update(ctx);
    return (flags & MPV_RENDER_UPDATE_FRAME) != 0;
}

bool mpv_handle_t::copy_sw_pixels(uint8_t *out, int out_size, int *out_width, int *out_height) {
    std::lock_guard<std::mutex> lock(resize_mutex_);
    if (!sw_front_buffer_ || sw_width_ <= 0 || sw_height_ <= 0) return false;
    int size = sw_width_ * sw_height_ * 4;
    if (out_size < size) return false;
    memcpy(out, sw_front_buffer_, size);
    *out_width = sw_width_;
    *out_height = sw_height_;
    return true;
}

bool mpv_handle_t::render_sw_frame_to_direct(uint8_t *direct_ptr, int buf_size, int *out_w, int *out_h) {
    if (!sw_render_ctx_ || !direct_ptr) return false;

    int w = sw_width_;
    int h = sw_height_;
    int needed = w * h * 4;
    if (w <= 0 || h <= 0 || buf_size < needed) return false;

    int32_t size[] = {w, h};
    int pitch = 4 * w;

    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_SW_SIZE, size},
        {MPV_RENDER_PARAM_SW_FORMAT, const_cast<char*>("bgra")},
        {MPV_RENDER_PARAM_SW_STRIDE, &pitch},
        {MPV_RENDER_PARAM_SW_POINTER, direct_ptr},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };

    int result = mpv_render_context_render(sw_render_ctx_, params);
    if (result < 0) return false;

    *out_w = w;
    *out_h = h;
    return true;
}

// OpenGL render context (GPU-accelerated)

bool mpv_handle_t::create_gl_render_context(int width, int height) {
    FP;
    CHECK_HANDLE()

    if (gl_render_ctx_) {
        destroy_gl_render_context();
    }

    gl_render_ctx_ = new render_context_t(handle_, width, height);
    if (!gl_render_ctx_->isValid()) {
        LOG("Failed to create GL render context");
        delete gl_render_ctx_;
        gl_render_ctx_ = nullptr;
        return false;
    }

    LOG("GL render context created: %dx%d", width, height);
    return true;
}

bool mpv_handle_t::render_gl_frame() {
    if (!gl_render_ctx_) return false;
    return gl_render_ctx_->render();
}

void mpv_handle_t::destroy_gl_render_context() {
    if (gl_render_ctx_) {
        delete gl_render_ctx_;
        gl_render_ctx_ = nullptr;
    }
    // Wake up any thread blocked in wait_for_frame()
    frame_cv_.notify_all();
}

bool mpv_handle_t::copy_gl_pixels(uint8_t *out, int out_size, int *out_width, int *out_height) {
    if (!gl_render_ctx_) return false;
    int w = gl_render_ctx_->getWidth();
    int h = gl_render_ctx_->getHeight();
    int size = w * h * 4;
    if (out_size < size) return false;
    const uint8_t *pixels = gl_render_ctx_->getPixelBuffer();
    if (!pixels) return false;
    memcpy(out, pixels, size);
    *out_width = w;
    *out_height = h;
    return true;
}

int mpv_handle_t::get_gl_width() const {
    return gl_render_ctx_ ? gl_render_ctx_->getWidth() : 0;
}

int mpv_handle_t::get_gl_height() const {
    return gl_render_ctx_ ? gl_render_ctx_->getHeight() : 0;
}

// ANGLE render context (GPU-accelerated via D3D11)

bool mpv_handle_t::create_angle_render_context(int width, int height) {
    FP;
    CHECK_HANDLE()

    if (angle_render_ctx_) {
        destroy_angle_render_context();
    }

    angle_render_ctx_ = new angle_render_context_t(handle_, width, height);
    if (!angle_render_ctx_->isValid()) {
        LOG("Failed to create ANGLE render context");
        delete angle_render_ctx_;
        angle_render_ctx_ = nullptr;
        return false;
    }

    // Register update callback for event-driven rendering
    mpv_render_context *render_ctx = angle_render_ctx_->getMpvRenderContext();
    if (render_ctx) {
        mpv_render_context_set_update_callback(render_ctx, [](void *ctx) {
            auto *self = static_cast<mpv_handle_t*>(ctx);
            {
                std::lock_guard<std::mutex> lock(self->frame_mutex_);
                self->frame_ready_ = true;
            }
            self->frame_cv_.notify_one();
        }, this);
    }

    LOG("ANGLE render context created: %dx%d", width, height);
    return true;
}

bool mpv_handle_t::resize_angle_render_context(int width, int height) {
    if (!angle_render_ctx_) return false;
    return angle_render_ctx_->resize(width, height);
}

bool mpv_handle_t::render_angle_frame() {
    if (!angle_render_ctx_) return false;
    return angle_render_ctx_->render();
}

void mpv_handle_t::destroy_angle_render_context() {
    if (angle_render_ctx_) {
        delete angle_render_ctx_;
        angle_render_ctx_ = nullptr;
    }
    // Wake up any thread blocked in wait_for_frame()
    frame_cv_.notify_all();
}

bool mpv_handle_t::copy_angle_pixels(uint8_t *out, int out_size, int *out_width, int *out_height) {
    if (!angle_render_ctx_) return false;
    int w = angle_render_ctx_->getWidth();
    int h = angle_render_ctx_->getHeight();
    int size = w * h * 4;
    if (out_size < size) return false;
    const uint8_t *pixels = angle_render_ctx_->getPixelBuffer();
    if (!pixels) return false;
    memcpy(out, pixels, size);
    *out_width = w;
    *out_height = h;
    return true;
}

int mpv_handle_t::get_angle_width() const {
    return angle_render_ctx_ ? angle_render_ctx_->getWidth() : 0;
}

int mpv_handle_t::get_angle_height() const {
    return angle_render_ctx_ ? angle_render_ctx_->getHeight() : 0;
}

bool mpv_handle_t::is_angle_available() const {
    return angle_render_ctx_ != nullptr;
}

bool mpv_handle_t::destroy(JNIEnv *env) {
    FP;
    CHECK_HANDLE()

    // Destroy render contexts before mpv handle
    destroy_angle_render_context();
    destroy_gl_render_context();
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
