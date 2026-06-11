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
    bool render_sw_frame();
    void destroy_sw_render_context();
    const uint8_t *get_sw_pixels() const;
    int get_sw_width() const;
    int get_sw_height() const;
    int get_video_width() const;
    int get_video_height() const;
    void wait_for_frame();

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
    uint8_t *sw_pixel_buffer_ = nullptr;
    int sw_width_ = 0;
    int sw_height_ = 0;
    int video_width_ = 0;
    int video_height_ = 0;

    // Event-driven rendering synchronization
    std::mutex frame_mutex_;
    std::condition_variable frame_cv_;
    bool frame_ready_ = false;

    std::shared_ptr<mediampv::compatible_thread> event_thread_;
    bool event_loop_request_exit = false;

    void *event_loop(void *arg);
};

} // namespace mediampv

#endif //MEDIAMP_MPV_HANDLE_T_H
