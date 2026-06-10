//
// Desktop MPV render context for offscreen OpenGL rendering
//

#ifndef MEDIAMP_RENDER_CONTEXT_T_H
#define MEDIAMP_RENDER_CONTEXT_T_H

#include <mpv/client.h>
#include <mpv/render.h>
#include <cstdint>

namespace mediampv {

class render_context_t {
public:
    render_context_t(mpv_handle *mpv, int width, int height);
    ~render_context_t();

    bool isValid() const { return render_ctx_ != nullptr; }

    // Render a frame and read pixels into BGRA buffer
    // Returns true if a new frame was rendered
    bool render();

    // Get the pixel buffer (BGRA format, width*height*4 bytes)
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

    // OpenGL objects
    unsigned int fbo_ = 0;
    unsigned int texture_ = 0;
    unsigned int rbo_ = 0;

    uint8_t *pixel_buffer_ = nullptr;

    // Platform-specific OpenGL context
    void *gl_context_ = nullptr;
    void *gl_display_ = nullptr;

    bool initOpenGL();
    void cleanupOpenGL();
    bool createFramebuffer();
    void destroyFramebuffer();
};

} // namespace mediampv

#endif // MEDIAMP_RENDER_CONTEXT_T_H
