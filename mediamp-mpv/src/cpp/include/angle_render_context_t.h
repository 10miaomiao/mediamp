//
// ANGLE render context - uses EGL + ANGLE D3D11 backend with OpenGL ES 3.0
// for GPU-accelerated mpv rendering on Windows
//

#ifndef MEDIAMP_ANGLE_RENDER_CONTEXT_T_H
#define MEDIAMP_ANGLE_RENDER_CONTEXT_T_H

#include <mpv/client.h>
#include <mpv/render.h>
#include <mpv/render_gl.h>
#include <cstdint>

namespace mediampv {

class angle_render_context_t {
public:
    angle_render_context_t(mpv_handle *mpv, int width, int height);
    ~angle_render_context_t();

    bool isValid() const { return render_ctx_ != nullptr; }
    mpv_render_context *getMpvRenderContext() const { return render_ctx_; }

    // Render a frame and read pixels into BGRA buffer
    bool render();

    // Get the pixel buffer (BGRA format)
    const uint8_t *getPixelBuffer() const { return pixel_buffer_; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

    // Resize the render target
    bool resize(int width, int height);

private:
    mpv_handle *mpv_;
    mpv_render_context *render_ctx_ = nullptr;

    int width_;
    int height_;

    // EGL state
    void *egl_display_ = nullptr;  // EGLDisplay
    void *egl_context_ = nullptr;  // EGLContext
    void *egl_surface_ = nullptr;  // EGLSurface (pbuffer)

    // GLES objects
    unsigned int fbo_ = 0;
    unsigned int texture_ = 0;
    unsigned int rbo_ = 0;

    uint8_t *pixel_buffer_ = nullptr;

    void *saved_config_ = nullptr;    // EGLConfig saved for resize
    bool use_bgra_readback_ = false;  // Whether GL_BGRA_EXT readback is supported

    bool initEGL();
    void cleanupEGL();
    bool createFramebuffer();
    void destroyFramebuffer();
};

} // namespace mediampv

#endif // MEDIAMP_ANGLE_RENDER_CONTEXT_T_H
