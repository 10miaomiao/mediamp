//
// Created by StageGuard on 12/28/2024.
//

#ifndef MEDIAMP_MPV_HANDLE_T_H
#define MEDIAMP_MPV_HANDLE_T_H

#include <iostream>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <jni.h>
#include <mpv/client.h>
#include <mpv/render.h>
#include "compatible_thread.h"
#include "log.h"
#include "render_context_t.h"
#include "angle_render_context_t.h"

namespace mediampv {

class mpv_handle_t final {
public:
    explicit mpv_handle_t(JNIEnv *env, jobject app_context) {
        create(env, app_context);
    }
    ~mpv_handle_t() = default;
    
    void create(JNIEnv *env, jobject app_context);
    bool initialize();
    bool set_event_listener(JNIEnv *env, jobject listener);
    bool destroy(JNIEnv *env);
    
    bool command(const char **args);
    bool set_option(const char *key, const char *value);
    bool get_property(const char *name, mpv_format format, void *out_result);
    bool set_property(const char *name, mpv_format format, void *in_value);
    bool observe_property(const char *property, mpv_format format, uint64_t reply_data);
    bool unobserve_property(uint64_t reply_data);
    
    bool attach_android_surface(JNIEnv *env, jobject surface);
    bool detach_android_surface(JNIEnv *env);

    // Software render context (vo=libmpv)
    bool create_sw_render_context(int width, int height);
    bool resize_sw_render_context(int width, int height);
    bool render_sw_frame();
    void destroy_sw_render_context();
    const uint8_t *get_sw_pixels() const;
    int get_sw_width() const;
    int get_sw_height() const;
    int get_video_width() const;
    int get_video_height() const;
    bool query_video_resolution(int *out_w, int *out_h);
    void wait_for_frame();
    bool has_pending_frame();
    bool copy_sw_pixels(uint8_t *out, int out_size, int *out_width, int *out_height);

    // DirectByteBuffer render: mpv renders directly to a Java DirectByteBuffer
    bool render_sw_frame_to_direct(uint8_t *direct_ptr, int buf_size, int *out_w, int *out_h);

    // OpenGL render context (GPU-accelerated via WGL)
    bool create_gl_render_context(int width, int height);
    bool render_gl_frame();
    void destroy_gl_render_context();
    bool copy_gl_pixels(uint8_t *out, int out_size, int *out_width, int *out_height);
    int get_gl_width() const;
    int get_gl_height() const;

    // GL shared context (for GPU texture export to Skia)
    void *create_gl_shared_context();
    void *get_gl_hdc();
    unsigned int copy_gl_texture_to_new(int *outWidth, int *outHeight);
    void finish_gl_render();
    bool is_gl_available() const { return gl_render_ctx_ != nullptr; }
    unsigned int get_gl_fbo_id() const;

    // ANGLE render context (GPU-accelerated via D3D11)
    bool create_angle_render_context(int width, int height);
    bool resize_angle_render_context(int width, int height);
    bool render_angle_frame();
    void destroy_angle_render_context();
    bool copy_angle_pixels(uint8_t *out, int out_size, int *out_width, int *out_height);
    int get_angle_width() const;
    int get_angle_height() const;
    bool is_angle_available() const;
    void *get_angle_shared_texture_handle() const;
    void *get_angle_d3d11_device() const;
    bool read_angle_pixels(uint8_t *out, int out_size, int *out_width, int *out_height);
    bool begin_read_pixels();
    bool get_read_pixels_result(uint8_t *out, int out_size, int *out_width, int *out_height);

    // Get the ANGLE render context (for fence operations)
    angle_render_context_t *get_angle_render_context() const { return angle_render_ctx_; }

private:
    JavaVM *jvm_;
    mpv_handle *handle_;

    jobject *event_listener_ = nullptr;

#ifdef __ANDROID__
    bool surface_attached_ = false;
    jobject surface_;
#endif

    // Software render context (vo=libmpv)
    mpv_render_context *sw_render_ctx_ = nullptr;
    uint8_t *sw_pixel_buffer_ = nullptr;   // back buffer (mpv writes here)
    uint8_t *sw_front_buffer_ = nullptr;   // front buffer (display reads here)
    int sw_width_ = 0;
    int sw_height_ = 0;
    int video_width_ = 0;
    int video_height_ = 0;

    // OpenGL render context (GPU-accelerated via WGL)
    render_context_t *gl_render_ctx_ = nullptr;

    // ANGLE render context (GPU-accelerated via D3D11)
    angle_render_context_t *angle_render_ctx_ = nullptr;

    // Event-driven rendering synchronization
    std::mutex frame_mutex_;
    std::condition_variable frame_cv_;
    bool frame_ready_ = false;

    // Protects sw_pixel_buffer_ / sw_front_buffer_ / sw_width_ / sw_height_
    // during resize (event thread) and render/copy (render thread)
    std::mutex resize_mutex_;

    std::shared_ptr<mediampv::compatible_thread> event_thread_;
    bool event_loop_request_exit = false;

    void *event_loop(void *arg);
};

} // namespace mediampv

#endif //MEDIAMP_MPV_HANDLE_T_H
