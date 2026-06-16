//
// ANGLE render context - EGL + ANGLE D3D11 backend with OpenGL ES 3.0
// Renders to D3D11 shared textures via eglCreatePbufferFromClientBuffer
// for zero-copy GPU texture export to Skiko/D3D12.
//

#include "include/angle_render_context_t.h"
#include "include/log.h"
#include <mpv/render_gl.h>
#include <cstring>
#include <cstdlib>

// EGL + GLES headers
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglext_angle.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3platform.h>

// D3D11 headers
#include <d3d11.h>
#include <dxgi.h>

// ANGLE D3D11 display type constant
#ifndef EGL_D3D11_ONLY_DISPLAY_ANGLE
#define EGL_D3D11_ONLY_DISPLAY_ANGLE ((EGLNativeDisplayType)-3)
#endif

// EGL platform extension function pointer
typedef EGLDisplay (EGLAPIENTRYP PFNEGLGETPLATFORMDISPLAYEXTPROC)(EGLenum platform, void *native_display, const EGLint *attrib_list);

// EGL device query function pointers
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYDISPLAYATTRIBEXTPROC)(EGLDisplay dpy, EGLint attribute, EGLAttrib *value);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYDEVICEATTRIBEXTPROC)(EGLDeviceEXT device, EGLint attribute, EGLAttrib *value);

// EGL_D3D_TEXTURE_ANGLE constant
#ifndef EGL_D3D_TEXTURE_ANGLE
#define EGL_D3D_TEXTURE_ANGLE 0x33A3
#endif

// EGL_DEVICE_EXT constant
#ifndef EGL_DEVICE_EXT
#define EGL_DEVICE_EXT 0x322C
#endif

// EGL_D3D11_DEVICE_ANGLE constant
#ifndef EGL_D3D11_DEVICE_ANGLE
#define EGL_D3D11_DEVICE_ANGLE 0x33A1
#endif

namespace mediampv {

bool angle_render_context_t::initEGL() {
    // Initialize EGL display with ANGLE D3D11 backend
    EGLDisplay display = EGL_NO_DISPLAY;

    // Try eglGetPlatformDisplayEXT with D3D11 backend
    auto eglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC)
        eglGetProcAddress("eglGetPlatformDisplayEXT");

    if (eglGetPlatformDisplayEXT) {
        display = eglGetPlatformDisplayEXT(
            EGL_PLATFORM_ANGLE_ANGLE,
            (void*)EGL_D3D11_ONLY_DISPLAY_ANGLE,
            nullptr
        );
        LOG("ANGLE: Using D3D11 backend via eglGetPlatformDisplayEXT");
    }

    if (display == EGL_NO_DISPLAY) {
        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        LOG("ANGLE: Using default display (fallback)");
    }

    if (display == EGL_NO_DISPLAY) {
        LOG("ANGLE: Failed to get EGL display");
        return false;
    }

    EGLint major, minor;
    if (!eglInitialize(display, &major, &minor)) {
        LOG("ANGLE: Failed to initialize EGL, error=0x%X", eglGetError());
        return false;
    }
    LOG("ANGLE: EGL initialized: version %d.%d", major, minor);

    // Configure for pbuffer surface with RGBA8
    EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };

    EGLConfig config;
    EGLint num_configs;
    if (!eglChooseConfig(display, config_attribs, &config, 1, &num_configs) || num_configs == 0) {
        LOG("ANGLE: Failed to choose EGL config, error=0x%X", eglGetError());
        eglTerminate(display);
        return false;
    }

    saved_config_ = (void*)config;

    // Bind OpenGL ES API
    eglBindAPI(EGL_OPENGL_ES_API);

    // Create OpenGL ES 3.0 context
    EGLint context_attribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 0,
        EGL_NONE
    };
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs);
    if (context == EGL_NO_CONTEXT) {
        LOG("ANGLE: Failed to create GLES 3.0 context, error=0x%X", eglGetError());
        eglTerminate(display);
        return false;
    }

    egl_display_ = (void*)display;
    egl_context_ = (void*)context;

    // Query the D3D11 device from ANGLE
    auto eglQueryDisplayAttribEXT = (PFNEGLQUERYDISPLAYATTRIBEXTPROC)
        eglGetProcAddress("eglQueryDisplayAttribEXT");
    auto eglQueryDeviceAttribEXT = (PFNEGLQUERYDEVICEATTRIBEXTPROC)
        eglGetProcAddress("eglQueryDeviceAttribEXT");

    if (eglQueryDisplayAttribEXT && eglQueryDeviceAttribEXT) {
        EGLAttrib device = 0;
        if (eglQueryDisplayAttribEXT(display, EGL_DEVICE_EXT, &device)) {
            EGLAttrib d3d11_device_ptr = 0;
            if (eglQueryDeviceAttribEXT((EGLDeviceEXT)device, EGL_D3D11_DEVICE_ANGLE, &d3d11_device_ptr)) {
                d3d11_device_ = (ID3D11Device*)d3d11_device_ptr;
                if (d3d11_device_) {
                    d3d11_device_->AddRef();
                    LOG("ANGLE: Got D3D11 device from ANGLE: %p", d3d11_device_);
                }
            }
        }
    }

    if (!d3d11_device_) {
        LOG("ANGLE: Failed to get D3D11 device from ANGLE");
    }

    // Log GL info
    // Make context current temporarily to read GL info
    // We need a temporary pbuffer for this
    EGLint pbuffer_attribs[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
    EGLSurface tmp_surface = eglCreatePbufferSurface(display, config, pbuffer_attribs);
    if (tmp_surface != EGL_NO_SURFACE) {
        eglMakeCurrent(display, tmp_surface, tmp_surface, context);
        const char *vendor = (const char*)glGetString(GL_VENDOR);
        const char *renderer = (const char*)glGetString(GL_RENDERER);
        const char *version = (const char*)glGetString(GL_VERSION);
        LOG("ANGLE: GL_VENDOR=%s, GL_RENDERER=%s, GL_VERSION=%s",
            vendor ? vendor : "null", renderer ? renderer : "null", version ? version : "null");
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroySurface(display, tmp_surface);
    }

    return true;
}

void angle_render_context_t::cleanupEGL() {
    if (egl_display_) {
        auto display = (EGLDisplay)egl_display_;
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (egl_context_) {
            eglDestroyContext(display, (EGLContext)egl_context_);
            egl_context_ = nullptr;
        }
        eglTerminate(display);
        egl_display_ = nullptr;
    }
    if (d3d11_device_) {
        d3d11_device_->Release();
        d3d11_device_ = nullptr;
    }
}

bool angle_render_context_t::createSharedTextures() {
    if (!d3d11_device_) {
        LOG("ANGLE: No D3D11 device, cannot create shared textures");
        return false;
    }

    auto display = (EGLDisplay)egl_display_;
    auto config = (EGLConfig)saved_config_;

    for (int i = 0; i < BUF_COUNT; i++) {
        // Create D3D11 shared texture
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width_;
        desc.Height = height_;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET;
        desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

        HRESULT hr = d3d11_device_->CreateTexture2D(&desc, nullptr, &d3d11_textures_[i]);
        if (FAILED(hr)) {
            LOG("ANGLE: Failed to create D3D11 shared texture %d, hr=0x%lX", i, hr);
            return false;
        }

        // Get shared HANDLE via DXGI
        IDXGIResource *dxgiRes = nullptr;
        hr = d3d11_textures_[i]->QueryInterface(__uuidof(IDXGIResource), (void**)&dxgiRes);
        if (FAILED(hr)) {
            LOG("ANGLE: Failed to QueryInterface IDXGIResource for texture %d, hr=0x%lX", i, hr);
            d3d11_textures_[i]->Release();
            d3d11_textures_[i] = nullptr;
            return false;
        }
        hr = dxgiRes->GetSharedHandle(&shared_handles_[i]);
        dxgiRes->Release();
        if (FAILED(hr) || !shared_handles_[i]) {
            LOG("ANGLE: Failed to get shared handle for texture %d, hr=0x%lX", i, hr);
            d3d11_textures_[i]->Release();
            d3d11_textures_[i] = nullptr;
            return false;
        }

        // Create EGL pbuffer surface from D3D11 texture
        EGLint pbuffer_attributes[] = {
            EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA,
            EGL_TEXTURE_TARGET, EGL_TEXTURE_2D,
            EGL_NONE,
        };
        egl_surfaces_[i] = eglCreatePbufferFromClientBuffer(
            display, EGL_D3D_TEXTURE_ANGLE,
            (EGLClientBuffer)d3d11_textures_[i],
            config, pbuffer_attributes);

        if (egl_surfaces_[i] == EGL_NO_SURFACE) {
            LOG("ANGLE: Failed to create pbuffer from D3D11 texture %d, error=0x%X", i, eglGetError());
            d3d11_textures_[i]->Release();
            d3d11_textures_[i] = nullptr;
            shared_handles_[i] = nullptr;
            return false;
        }

        LOG("ANGLE: Created shared texture %d: %dx%d, handle=%p, pbuffer=%p",
            i, width_, height_, shared_handles_[i], egl_surfaces_[i]);
    }

    // Make the write buffer's pbuffer current (to verify surfaces work)
    if (!eglMakeCurrent(display, (EGLSurface)egl_surfaces_[write_idx_],
                   (EGLSurface)egl_surfaces_[write_idx_], (EGLContext)egl_context_)) {
        LOG("ANGLE: Initial eglMakeCurrent failed, error=0x%X", eglGetError());
        return false;
    }

    return true;
}

void angle_render_context_t::destroySharedTextures() {
    releaseReadbackCache();
    auto display = (EGLDisplay)egl_display_;

    for (int i = 0; i < BUF_COUNT; i++) {
        if (egl_surfaces_[i]) {
            if (display) {
                eglDestroySurface(display, (EGLSurface)egl_surfaces_[i]);
            }
            egl_surfaces_[i] = nullptr;
        }
        if (d3d11_textures_[i]) {
            d3d11_textures_[i]->Release();
            d3d11_textures_[i] = nullptr;
        }
        shared_handles_[i] = nullptr;
    }
}

bool angle_render_context_t::createFallbackFBO() {
    // Fallback: standard GL FBO (used if shared texture creation fails)
    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOG("ANGLE: Fallback FBO not complete: 0x%X", status);
        destroyFallbackFBO();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    pixel_buffer_ = new uint8_t[width_ * height_ * 4];
    return true;
}

void angle_render_context_t::destroyFallbackFBO() {
    if (fbo_) {
        glDeleteFramebuffers(1, &fbo_);
        fbo_ = 0;
    }
    if (texture_) {
        glDeleteTextures(1, &texture_);
        texture_ = 0;
    }
    if (pixel_buffer_) {
        delete[] pixel_buffer_;
        pixel_buffer_ = nullptr;
    }
}

angle_render_context_t::angle_render_context_t(mpv_handle *mpv, int width, int height)
    : mpv_(mpv), width_(width), height_(height) {

    if (!initEGL()) {
        LOG("ANGLE: Failed to initialize EGL context");
        return;
    }

    // Try shared texture path first
    if (d3d11_device_ && createSharedTextures()) {
        use_shared_texture_ = true;
        LOG("ANGLE: Using D3D11 shared texture path (GPU export enabled)");
    } else {
        // Fallback to GL FBO + glReadPixels
        use_shared_texture_ = false;
        LOG("ANGLE: Shared texture creation failed, falling back to FBO + glReadPixels");

        // Make context current with a temporary pbuffer for FBO creation
        auto display = (EGLDisplay)egl_display_;
        auto config = (EGLConfig)saved_config_;
        EGLint pbuffer_attribs[] = { EGL_WIDTH, width_, EGL_HEIGHT, height_, EGL_NONE };
        EGLSurface tmp_surface = eglCreatePbufferSurface(display, config, pbuffer_attribs);
        if (tmp_surface == EGL_NO_SURFACE) {
            LOG("ANGLE: Failed to create fallback pbuffer");
            cleanupEGL();
            return;
        }
        eglMakeCurrent(display, tmp_surface, tmp_surface, (EGLContext)egl_context_);
        // Store as the single surface for fallback
        egl_surfaces_[0] = tmp_surface;

        if (!createFallbackFBO()) {
            LOG("ANGLE: Failed to create fallback FBO");
            cleanupEGL();
            return;
        }
    }

    // Create mpv render context
    mpv_opengl_init_params gl_init_params = {
        .get_proc_address = [](void *ctx, const char *name) -> void* {
            return (void*)eglGetProcAddress(name);
        },
        .get_proc_address_ctx = nullptr,
    };

    int advanced_control = 1;
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params},
        {MPV_RENDER_PARAM_ADVANCED_CONTROL, &advanced_control},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };

    int result = mpv_render_context_create(&render_ctx_, mpv_, params);
    if (result < 0) {
        LOG("ANGLE: Failed to create mpv render context: %d", result);
        if (use_shared_texture_) {
            destroySharedTextures();
        } else {
            destroyFallbackFBO();
        }
        cleanupEGL();
        render_ctx_ = nullptr;
        return;
    }

    LOG("ANGLE: MPV render context created: %dx%d (shared_texture=%d)",
        width_, height_, use_shared_texture_);

    // Release EGL context from the main/initialization thread so that
    // the mpv render thread can claim it via eglMakeCurrent in render().
    {
        auto display = (EGLDisplay)egl_display_;
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
}

angle_render_context_t::~angle_render_context_t() {
    if (render_ctx_) {
        mpv_render_context_free(render_ctx_);
    }
    if (use_shared_texture_) {
        destroySharedTextures();
    } else {
        destroyFallbackFBO();
    }
    cleanupEGL();
}

bool angle_render_context_t::render() {
    if (!render_ctx_) {
        LOG("render: render_ctx_ is null");
        return false;
    }

    auto display = (EGLDisplay)egl_display_;
    static int render_call_count = 0;
    render_call_count++;
    if (render_call_count <= 3 || render_call_count % 100 == 0) {
        LOG("render: call #%d, display=%p, surface=%p, context=%p, write_idx=%d",
            render_call_count, display, egl_surfaces_[write_idx_], egl_context_, write_idx_);
    }

    if (use_shared_texture_) {
        // Shared texture path: render to pbuffer backed by D3D11 texture
        if (!eglMakeCurrent(display, (EGLSurface)egl_surfaces_[write_idx_],
                       (EGLSurface)egl_surfaces_[write_idx_], (EGLContext)egl_context_)) {
            EGLint err = eglGetError();
            LOG("render: eglMakeCurrent failed, error=0x%X, display=%p, surface=%p, context=%p",
                err, display, egl_surfaces_[write_idx_], egl_context_);
            return false;
        }
        if (render_call_count <= 3) {
            LOG("render: eglMakeCurrent succeeded, calling mpv_render_context_render");
        }

        mpv_opengl_fbo fbo_params = {
            .fbo = 0,  // default framebuffer = pbuffer = D3D11 texture
            .w = width_,
            .h = height_,
            .internal_format = GL_RGBA8,
        };

        int flip_y = 1;
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_OPENGL_FBO, &fbo_params},
            {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };

        int result = mpv_render_context_render(render_ctx_, params);
        if (result < 0) {
            LOG("render: mpv_render_context_render failed, result=%d", result);
            return false;
        }
        if (render_call_count <= 3) {
            LOG("render: mpv_render_context_render succeeded, frame #%d", render_call_count);
        }

        // Ensure D3D11 GPU operations complete before D3D12 reads
        glFlush();
        eglWaitClient();

        // Swap double-buffer: write becomes read, read becomes write
        int new_read = write_idx_;
        write_idx_ = read_idx_;
        read_idx_ = new_read;

        return true;
    } else {
        // Fallback: render to FBO, read pixels
        eglMakeCurrent(display, (EGLSurface)egl_surfaces_[0],
                       (EGLSurface)egl_surfaces_[0], (EGLContext)egl_context_);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glViewport(0, 0, width_, height_);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        mpv_opengl_fbo fbo_params = {
            .fbo = static_cast<int>(fbo_),
            .w = width_,
            .h = height_,
            .internal_format = GL_RGBA8,
        };

        int flip_y = 1;
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_OPENGL_FBO, &fbo_params},
            {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };

        int result = mpv_render_context_render(render_ctx_, params);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return result >= 0;
    }
}

void *angle_render_context_t::getSharedTextureHandle() const {
    if (!use_shared_texture_) return nullptr;
    return shared_handles_[read_idx_];
}

bool angle_render_context_t::readPixels(uint8_t *out, int out_size, int *out_width, int *out_height) {
    if (!use_shared_texture_ || !d3d11_device_) {
        LOG("readPixels: failed, use_shared_texture_=%d, d3d11_device_=%p", use_shared_texture_, d3d11_device_);
        return false;
    }

    // Lazily create a separate D3D11 device for readback (avoids contention with ANGLE's device)
    if (!readback_device_) {
        D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
        D3D_FEATURE_LEVEL obtainedLevel;
        HRESULT hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            0, featureLevels, 1,
            D3D11_SDK_VERSION,
            &readback_device_, &obtainedLevel, &readback_ctx_);
        if (FAILED(hr) || !readback_device_) {
            LOG("readPixels: Failed to create readback D3D11 device, hr=0x%lX", hr);
            return false;
        }
        LOG("readPixels: Created separate D3D11 readback device");
    }

    int read_idx = read_idx_;
    void *sharedHandle = shared_handles_[read_idx];
    if (!sharedHandle) return false;

    // Open shared texture on the readback device (cache by index)
    if (!cached_shared_textures_[read_idx]) {
        HRESULT hr = readback_device_->OpenSharedResource(
            sharedHandle, __uuidof(ID3D11Texture2D), (void**)&cached_shared_textures_[read_idx]);
        if (FAILED(hr) || !cached_shared_textures_[read_idx]) {
            LOG("readPixels: OpenSharedResource failed, hr=0x%lX", hr);
            return false;
        }
    }
    ID3D11Texture2D *src = cached_shared_textures_[read_idx];

    // Recreate staging texture if size changed
    if (!staging_texture_ || staging_width_ != width_ || staging_height_ != height_) {
        if (staging_texture_) {
            staging_texture_->Release();
            staging_texture_ = nullptr;
        }
        D3D11_TEXTURE2D_DESC desc;
        src->GetDesc(&desc);
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.MiscFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        HRESULT hr = readback_device_->CreateTexture2D(&desc, nullptr, &staging_texture_);
        if (FAILED(hr) || !staging_texture_) {
            LOG("readPixels: CreateTexture2D staging failed, hr=0x%lX", hr);
            return false;
        }
        staging_width_ = width_;
        staging_height_ = height_;
    }

    // Copy and map using the readback device's own context (no contention with ANGLE)
    readback_ctx_->CopyResource(staging_texture_, src);

    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = readback_ctx_->Map(staging_texture_, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        LOG("readPixels: Map failed, hr=0x%lX", hr);
        return false;
    }

    int w = width_;
    int h = height_;
    int needed = w * h * 4;
    if (out_size < needed) {
        readback_ctx_->Unmap(staging_texture_, 0);
        return false;
    }

    uint8_t *src_ptr = (uint8_t*)mapped.pData;
    int rowBytes = w * 4;
    for (int row = 0; row < h; row++) {
        memcpy(out + row * rowBytes, src_ptr + row * mapped.RowPitch, rowBytes);
    }

    *out_width = w;
    *out_height = h;

    readback_ctx_->Unmap(staging_texture_, 0);
    return true;
}

void angle_render_context_t::releaseReadbackCache() {
    for (int i = 0; i < BUF_COUNT; i++) {
        if (cached_shared_textures_[i]) {
            cached_shared_textures_[i]->Release();
            cached_shared_textures_[i] = nullptr;
        }
    }
    if (staging_texture_) {
        staging_texture_->Release();
        staging_texture_ = nullptr;
    }
    if (readback_ctx_) {
        readback_ctx_->Release();
        readback_ctx_ = nullptr;
    }
    if (readback_device_) {
        readback_device_->Release();
        readback_device_ = nullptr;
    }
    staging_width_ = 0;
    staging_height_ = 0;
}

bool angle_render_context_t::resize(int width, int height) {
    if (width == width_ && height == height_) return true;
    if (width <= 0 || height <= 0) return false;

    width_ = width;
    height_ = height;

    if (use_shared_texture_) {
        destroySharedTextures();
        if (!createSharedTextures()) {
            LOG("ANGLE: Failed to resize shared textures, trying fallback");
            use_shared_texture_ = false;
            // Create fallback FBO
            auto display = (EGLDisplay)egl_display_;
            auto config = (EGLConfig)saved_config_;
            EGLint pbuffer_attribs[] = { EGL_WIDTH, width_, EGL_HEIGHT, height_, EGL_NONE };
            EGLSurface tmp_surface = eglCreatePbufferSurface(display, config, pbuffer_attribs);
            if (tmp_surface != EGL_NO_SURFACE) {
                egl_surfaces_[0] = tmp_surface;
                eglMakeCurrent(display, tmp_surface, tmp_surface, (EGLContext)egl_context_);
                createFallbackFBO();
            }
            return false;
        }
    } else {
        // Resize fallback FBO
        destroyFallbackFBO();
        auto display = (EGLDisplay)egl_display_;
        auto config = (EGLConfig)saved_config_;

        // Destroy old pbuffer
        if (egl_surfaces_[0]) {
            eglDestroySurface(display, (EGLSurface)egl_surfaces_[0]);
        }

        EGLint pbuffer_attribs[] = { EGL_WIDTH, width_, EGL_HEIGHT, height_, EGL_NONE };
        EGLSurface new_surface = eglCreatePbufferSurface(display, config, pbuffer_attribs);
        if (new_surface == EGL_NO_SURFACE) {
            LOG("ANGLE: Failed to create resized pbuffer");
            return false;
        }
        egl_surfaces_[0] = new_surface;
        eglMakeCurrent(display, new_surface, new_surface, (EGLContext)egl_context_);
        createFallbackFBO();
    }

    return true;
}

} // namespace mediampv
