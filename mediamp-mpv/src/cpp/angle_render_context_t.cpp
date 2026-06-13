//
// ANGLE render context - EGL + ANGLE D3D11 backend with OpenGL ES 3.0
// Uses ANGLE's D3D11 translation layer for reliable GPU rendering on Windows
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

// ANGLE D3D11 display type constant
#ifndef EGL_D3D11_ONLY_DISPLAY_ANGLE
#define EGL_D3D11_ONLY_DISPLAY_ANGLE ((EGLNativeDisplayType)-3)
#endif

// EGL platform extension function pointer
typedef EGLDisplay (EGLAPIENTRYP PFNEGLGETPLATFORMDISPLAYEXTPROC)(EGLenum platform, void *native_display, const EGLint *attrib_list);

// BGRA extension constants
#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif

namespace mediampv {

bool angle_render_context_t::initEGL() {
    // Initialize EGL display with ANGLE D3D11 backend
    EGLDisplay display = EGL_NO_DISPLAY;

    // Try eglGetPlatformDisplayEXT with D3D11 backend
    auto eglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC)
        eglGetProcAddress("eglGetPlatformDisplayEXT");

    if (eglGetPlatformDisplayEXT) {
        // Request D3D11-only backend from ANGLE
        display = eglGetPlatformDisplayEXT(
            EGL_PLATFORM_ANGLE_ANGLE,
            (void*)EGL_D3D11_ONLY_DISPLAY_ANGLE,
            nullptr
        );
        LOG("ANGLE: Using D3D11 backend via eglGetPlatformDisplayEXT");
    }

    if (display == EGL_NO_DISPLAY) {
        // Fallback: try EGL_DEFAULT_DISPLAY (ANGLE's default is usually D3D11)
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

    // Save config for resize operations
    saved_config_ = (void*)config;

    // Create pbuffer surface (offscreen rendering)
    EGLint pbuffer_attribs[] = {
        EGL_WIDTH, width_,
        EGL_HEIGHT, height_,
        EGL_NONE
    };
    EGLSurface surface = eglCreatePbufferSurface(display, config, pbuffer_attribs);
    if (surface == EGL_NO_SURFACE) {
        LOG("ANGLE: Failed to create pbuffer surface, error=0x%X", eglGetError());
        eglTerminate(display);
        return false;
    }

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
        eglDestroySurface(display, surface);
        eglTerminate(display);
        return false;
    }

    // Make context current
    if (!eglMakeCurrent(display, surface, surface, context)) {
        LOG("ANGLE: Failed to make EGL context current, error=0x%X", eglGetError());
        eglDestroyContext(display, context);
        eglDestroySurface(display, surface);
        eglTerminate(display);
        return false;
    }

    egl_display_ = (void*)display;
    egl_context_ = (void*)context;
    egl_surface_ = (void*)surface;

    // Check if BGRA readback is supported
    const char *exts = (const char*)glGetString(GL_EXTENSIONS);
    use_bgra_readback_ = exts && strstr(exts, "GL_EXT_read_format_bgra") != nullptr;
    LOG("ANGLE: BGRA readback %s", use_bgra_readback_ ? "supported" : "not supported, will convert RGBA->BGRA");

    // Log GL info
    const char *vendor = (const char*)glGetString(GL_VENDOR);
    const char *renderer = (const char*)glGetString(GL_RENDERER);
    const char *version = (const char*)glGetString(GL_VERSION);
    LOG("ANGLE: GL_VENDOR=%s, GL_RENDERER=%s, GL_VERSION=%s",
        vendor ? vendor : "null", renderer ? renderer : "null", version ? version : "null");

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
        if (egl_surface_) {
            eglDestroySurface(display, (EGLSurface)egl_surface_);
            egl_surface_ = nullptr;
        }
        eglTerminate(display);
        egl_display_ = nullptr;
    }
}

bool angle_render_context_t::createFramebuffer() {
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
        LOG("ANGLE: Framebuffer not complete: 0x%X", status);
        destroyFramebuffer();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void angle_render_context_t::destroyFramebuffer() {
    if (fbo_) {
        glDeleteFramebuffers(1, &fbo_);
        fbo_ = 0;
    }
    if (texture_) {
        glDeleteTextures(1, &texture_);
        texture_ = 0;
    }
    if (rbo_) {
        glDeleteRenderbuffers(1, &rbo_);
        rbo_ = 0;
    }
}

angle_render_context_t::angle_render_context_t(mpv_handle *mpv, int width, int height)
    : mpv_(mpv), width_(width), height_(height) {

    if (!initEGL()) {
        LOG("ANGLE: Failed to initialize EGL context");
        return;
    }

    if (!createFramebuffer()) {
        LOG("ANGLE: Failed to create framebuffer");
        cleanupEGL();
        return;
    }

    // Create mpv render context using GLES
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
        destroyFramebuffer();
        cleanupEGL();
        render_ctx_ = nullptr;
        return;
    }

    pixel_buffer_ = new uint8_t[width_ * height_ * 4];
    LOG("ANGLE: MPV render context created: %dx%d", width_, height_);
}

angle_render_context_t::~angle_render_context_t() {
    if (render_ctx_) {
        mpv_render_context_free(render_ctx_);
    }
    destroyFramebuffer();
    cleanupEGL();
    delete[] pixel_buffer_;
}

bool angle_render_context_t::render() {
    if (!render_ctx_ || !fbo_) return false;

    // Ensure EGL context is current on this thread
    if (egl_display_ && egl_surface_ && egl_context_) {
        auto display = (EGLDisplay)egl_display_;
        eglMakeCurrent(display, (EGLSurface)egl_surface_, (EGLSurface)egl_surface_, (EGLContext)egl_context_);
    }

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
    if (result < 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    // Read pixels - use BGRA directly on D3D11 (zero-copy), fallback to RGBA+convert
    if (use_bgra_readback_) {
        glReadPixels(0, 0, width_, height_, GL_BGRA_EXT, GL_UNSIGNED_BYTE, pixel_buffer_);
    } else {
        glReadPixels(0, 0, width_, height_, GL_RGBA, GL_UNSIGNED_BYTE, pixel_buffer_);
        // Convert RGBA to BGRA in-place (swap R and B channels)
        int pixel_count = width_ * height_;
        uint8_t *p = pixel_buffer_;
        for (int i = 0; i < pixel_count; i++, p += 4) {
            uint8_t tmp = p[0];
            p[0] = p[2];
            p[2] = tmp;
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return true;
}

bool angle_render_context_t::resize(int width, int height) {
    if (width == width_ && height == height_) return true;
    if (width <= 0 || height <= 0) return false;

    width_ = width;
    height_ = height;

    // Recreate framebuffer with new size
    destroyFramebuffer();
    if (!createFramebuffer()) return false;

    // Reallocate pixel buffer
    delete[] pixel_buffer_;
    pixel_buffer_ = new uint8_t[width_ * height_ * 4];

    // Recreate EGL pbuffer surface with new size
    if (egl_display_ && saved_config_ && egl_context_) {
        auto display = (EGLDisplay)egl_display_;
        auto config = (EGLConfig)saved_config_;
        auto old_surface = (EGLSurface)egl_surface_;

        // Detach old surface
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

        EGLint pbuffer_attribs[] = {
            EGL_WIDTH, width_,
            EGL_HEIGHT, height_,
            EGL_NONE
        };

        EGLSurface new_surface = eglCreatePbufferSurface(display, config, pbuffer_attribs);
        if (new_surface != EGL_NO_SURFACE) {
            eglDestroySurface(display, old_surface);
            egl_surface_ = (void*)new_surface;
            eglMakeCurrent(display, new_surface, new_surface, (EGLContext)egl_context_);
            LOG("ANGLE: Resized pbuffer to %dx%d", width_, height_);
        } else {
            // Restore old surface
            egl_surface_ = (void*)old_surface;
            eglMakeCurrent(display, old_surface, old_surface, (EGLContext)egl_context_);
            LOG("ANGLE: Failed to resize pbuffer, keeping old size");
            return false;
        }
    }

    return true;
}

} // namespace mediampv
