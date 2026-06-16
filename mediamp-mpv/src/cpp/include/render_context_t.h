//
// Desktop MPV render context for offscreen OpenGL rendering
//

#ifndef MEDIAMP_RENDER_CONTEXT_T_H
#define MEDIAMP_RENDER_CONTEXT_T_H

#include <mpv/client.h>
#include <mpv/render.h>
#include <mpv/render_gl.h>
#include <cstdint>

namespace mediampv {

class render_context_t {
public:
    render_context_t(mpv_handle *mpv, int width, int height);
    // 复用已有的隐藏窗口（用于与 vo=gpu 共享 OpenGL 上下文）
    render_context_t(mpv_handle *mpv, int width, int height, void *existing_hwnd);
    ~render_context_t();

    bool isValid() const { return render_ctx_ != nullptr; }
    mpv_render_context *getMpvRenderContext() const { return render_ctx_; }

    // Render a frame and read pixels into BGRA buffer
    // Returns true if a new frame was rendered
    bool render();

    // Get the pixel buffer (BGRA format, width*height*4 bytes)
    const uint8_t *getPixelBuffer() const { return pixel_buffer_; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

    // Resize the render target
    bool resize(int width, int height);

    // 获取隐藏窗口的 HWND（用于设置 mpv 的 wid 选项）
    void *getHiddenWindowHandle() const;

    // GPU texture export: create a shared WGL context from mpv's context.
    // The shared context can be made current on another thread (e.g. Skia render thread)
    // to access mpv's GL resources (textures, FBOs) via wglShareLists.
    // Returns opaque handle to the shared context (HGLRC), or nullptr on failure.
    void *createSharedGLContext();

    // Copy mpv's current FBO texture to a NEW GL texture.
    // Must be called on the render thread with the shared context current.
    // mpv must have already called render() + finishRender() before this.
    // Returns the new texture ID (owned by caller — Skia will delete via adoptTextureFrom).
    unsigned int copyToNewTexture(int *outWidth, int *outHeight);

    // Wait for the GPU to finish mpv's last render (glFinish on mpv's context).
    // Call this from the mpv render thread after render().
    void finishRender();

    // Get the GL texture ID of mpv's FBO (for shared context access).
    unsigned int getTextureId() const { return texture_; }

    // Get the GL FBO ID (for Skia BackendRenderTarget).
    unsigned int getFboId() const { return fbo_; }

    // Get mpv's HDC (for creating shared GL context on the same device).
    void *getHDC() const;

private:
    mpv_handle *mpv_;
    mpv_render_context *render_ctx_ = nullptr;

    int width_;
    int height_;

    // OpenGL objects
    unsigned int fbo_ = 0;
    unsigned int texture_ = 0;
    unsigned int rbo_ = 0;

    uint8_t *pixel_buffer_ = nullptr;

    // Platform-specific OpenGL context
    void *gl_context_ = nullptr;
    void *gl_display_ = nullptr;
    bool reuse_window_ = false;  // 是否复用外部窗口

    bool initOpenGL();
    bool initOpenGLWithExistingHwnd(void *hwnd);
    void cleanupOpenGL();
    bool createFramebuffer();
    void destroyFramebuffer();
};

} // namespace mediampv

#endif // MEDIAMP_RENDER_CONTEXT_T_H
