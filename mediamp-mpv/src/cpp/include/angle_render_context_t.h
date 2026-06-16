//
// ANGLE render context - uses EGL + ANGLE D3D11 backend with OpenGL ES 3.0
// for GPU-accelerated mpv rendering on Windows.
// Renders to D3D11 shared textures via eglCreatePbufferFromClientBuffer,
// enabling zero-copy GPU texture export to Skiko/D3D12.
//

#ifndef MEDIAMP_ANGLE_RENDER_CONTEXT_T_H
#define MEDIAMP_ANGLE_RENDER_CONTEXT_T_H

#include <mpv/client.h>
#include <mpv/render.h>
#include <mpv/render_gl.h>
#include <cstdint>

// Forward declare D3D11 types to avoid pulling in Windows headers
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;

namespace mediampv {

class angle_render_context_t {
public:
    angle_render_context_t(mpv_handle *mpv, int width, int height);
    ~angle_render_context_t();

    bool isValid() const { return render_ctx_ != nullptr; }
    mpv_render_context *getMpvRenderContext() const { return render_ctx_; }

    // Render a frame to the current write texture, then swap double-buffer.
    // Returns true on success.
    bool render();

    // Get the shared HANDLE of the current READ texture (for D3D12 import).
    // Returns 0 if not available.
    void *getSharedTextureHandle() const;

    // Get the D3D11 device pointer (for JNI to access).
    ID3D11Device *getD3D11Device() const { return d3d11_device_; }

    // Read pixels from the current read texture via cached staging texture.
    // Much faster than re-creating staging texture every frame.
    bool readPixels(uint8_t *out, int out_size, int *out_width, int *out_height);

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

    // D3D11 device (owned by ANGLE, we AddRef)
    ID3D11Device *d3d11_device_ = nullptr;

    // Double-buffered D3D11 shared textures and EGL pbuffers
    static const int BUF_COUNT = 2;
    ID3D11Texture2D *d3d11_textures_[BUF_COUNT] = {};
    void *egl_surfaces_[BUF_COUNT] = {};      // EGLSurface (pbuffer from D3D11 texture)
    void *shared_handles_[BUF_COUNT] = {};     // DXGI shared HANDLEs

    int write_idx_ = 0;  // Index of texture mpv writes to
    int read_idx_ = 1;   // Index of texture Skiko reads from

    void *saved_config_ = nullptr;  // EGLConfig saved for resize
    bool use_bgra_readback_ = false;  // Whether GL_BGRA_EXT is supported (unused in GPU path)

    // CPU readback fallback (used if shared texture path fails)
    uint8_t *pixel_buffer_ = nullptr;
    unsigned int fbo_ = 0;
    unsigned int texture_ = 0;
    bool use_shared_texture_ = true;  // false = fallback to FBO + glReadPixels

    // Cached readback resources (separate device to avoid ANGLE contention)
    ID3D11Device *readback_device_ = nullptr;
    ID3D11DeviceContext *readback_ctx_ = nullptr;
    ID3D11Texture2D *cached_shared_textures_[BUF_COUNT] = {};
    ID3D11Texture2D *staging_texture_ = nullptr;
    int staging_width_ = 0;
    int staging_height_ = 0;

    bool initEGL();
    void cleanupEGL();
    bool createSharedTextures();
    void destroySharedTextures();
    bool createFallbackFBO();
    void destroyFallbackFBO();
    void releaseReadbackCache();
};

} // namespace mediampv

#endif // MEDIAMP_ANGLE_RENDER_CONTEXT_T_H
