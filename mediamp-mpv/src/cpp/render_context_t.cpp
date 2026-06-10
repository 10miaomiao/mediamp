//
// Desktop MPV render context - offscreen OpenGL rendering
//

#include "include/render_context_t.h"
#include "include/log.h"
#include <cstring>
#include <cstdlib>

// Platform-specific OpenGL headers
#if defined(_WIN32) || defined(_WIN64)
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <GL/gl.h>
    // WGL function pointers
    typedef HGLRC (WINAPI *PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int*);
    typedef BOOL (WINAPI *PFNWGLCHOOSEPIXELFORMATARBPROC)(HDC, const int*, const FLOAT*, UINT, UINT*, UINT*);
    #define WGL_CONTEXT_MAJOR_VERSION_ARB     0x2091
    #define WGL_CONTEXT_MINOR_VERSION_ARB     0x2092
    #define WGL_CONTEXT_PROFILE_MASK_ARB      0x9126
    #define WGL_CONTEXT_CORE_PROFILE_BIT_ARB  0x00000001
    #define GL_FRAMEBUFFER                    0x8D40
    #define GL_COLOR_ATTACHMENT0              0x8CE0
    #define GL_RENDERBUFFER                   0x8D41
    #define GL_DEPTH_ATTACHMENT               0x8D00
    #define GL_DEPTH_COMPONENT24              0x81A6
#elif defined(__APPLE__)
    #include <OpenGL/gl.h>
    #include <OpenGL/CGLTypes.h>
    #include <OpenGL/CGLCurrent.h>
    #include <OpenGL/CGLContext.h>
#else
    #include <GL/gl.h>
    #include <GL/glx.h>
    #include <EGL/egl.h>
#endif

// GL function pointers (loaded at runtime on Windows)
#if defined(_WIN32) || defined(_WIN64)
typedef void (APIENTRY *PFNGLGENFRAMEBUFFERSPROC)(GLsizei, GLuint*);
typedef void (APIENTRY *PFNGLBINDFRAMEBUFFERPROC)(GLenum, GLuint);
typedef void (APIENTRY *PFNGLFRAMEBUFFERTEXTURE2DPROC)(GLenum, GLenum, GLenum, GLuint, GLint);
typedef void (APIENTRY *PFNGLGENRENDERBUFFERSPROC)(GLsizei, GLuint*);
typedef void (APIENTRY *PFNGLBINDRENDERBUFFERPROC)(GLenum, GLuint);
typedef void (APIENTRY *PFNGLRENDERBUFFERSTORAGEPROC)(GLenum, GLenum, GLsizei, GLsizei);
typedef void (APIENTRY *PFNGLFRAMEBUFFERRENDERBUFFERPROC)(GLenum, GLenum, GLenum, GLuint);
typedef void (APIENTRY *PFNGLDELETERENDERBUFFERSPROC)(GLsizei, const GLuint*);
typedef void (APIENTRY *PFNGLDELETEFRAMEBUFFERSPROC)(GLsizei, const GLuint*);
static PFNGLGENFRAMEBUFFERSPROC _glGenFramebuffers = nullptr;
static PFNGLBINDFRAMEBUFFERPROC _glBindFramebuffer = nullptr;
static PFNGLFRAMEBUFFERTEXTURE2DPROC _glFramebufferTexture2D = nullptr;
static PFNGLGENRENDERBUFFERSPROC _glGenRenderbuffers = nullptr;
static PFNGLBINDRENDERBUFFERPROC _glBindRenderbuffer = nullptr;
static PFNGLRENDERBUFFERSTORAGEPROC _glRenderbufferStorage = nullptr;
static PFNGLFRAMEBUFFERRENDERBUFFERPROC _glFramebufferRenderbuffer = nullptr;
static PFNGLDELETERENDERBUFFERSPROC _glDeleteRenderbuffers = nullptr;
static PFNGLDELETEFRAMEBUFFERSPROC _glDeleteFramebuffers = nullptr;
#define glGenFramebuffers _glGenFramebuffers
#define glBindFramebuffer _glBindFramebuffer
#define glFramebufferTexture2D _glFramebufferTexture2D
#define glGenRenderbuffers _glGenRenderbuffers
#define glBindRenderbuffer _glBindRenderbuffer
#define glRenderbufferStorage _glRenderbufferStorage
#define glFramebufferRenderbuffer _glFramebufferRenderbuffer
#define glDeleteRenderbuffers _glDeleteRenderbuffers
#define glDeleteFramebuffers _glDeleteFramebuffers
#else
// On Linux/macOS, use standard GL extensions
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#include <GL/glext.h>
#endif

namespace mediampv {

#if defined(_WIN32) || defined(_WIN64)

// Windows platform data
struct PlatformGLData {
    HWND hwnd = nullptr;
    HDC hdc = nullptr;
    HGLRC hglrc = nullptr;
    HINSTANCE opengl32 = nullptr;
};

static bool loadWGLExtensions(HDC hdc) {
    // We need a temporary context to load wglCreateContextAttribsARB
    HGLRC tempCtx = wglCreateContext(hdc);
    if (!tempCtx) return false;
    wglMakeCurrent(hdc, tempCtx);

    auto wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)
        wglGetProcAddress("wglCreateContextAttribsARB");

    if (!wglCreateContextAttribsARB) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(tempCtx);
        return false;
    }

    int attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0
    };

    HGLRC coreCtx = wglCreateContextAttribsARB(hdc, nullptr, attribs);
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(tempCtx);

    if (!coreCtx) return false;

    wglMakeCurrent(hdc, coreCtx);
    // Store the core context for later use
    return true;
}

static void loadGLFunctions() {
    _glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)wglGetProcAddress("glGenFramebuffers");
    _glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)wglGetProcAddress("glBindFramebuffer");
    _glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)wglGetProcAddress("glFramebufferTexture2D");
    _glGenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC)wglGetProcAddress("glGenRenderbuffers");
    _glBindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC)wglGetProcAddress("glBindRenderbuffer");
    _glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC)wglGetProcAddress("glRenderbufferStorage");
    _glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC)wglGetProcAddress("glFramebufferRenderbuffer");
    _glDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSPROC)wglGetProcAddress("glDeleteRenderbuffers");
    _glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)wglGetProcAddress("glDeleteFramebuffers");
}

#elif !defined(__APPLE__)

// Linux platform data
struct PlatformGLData {
    EGLDisplay egl_display = EGL_NO_DISPLAY;
    EGLContext egl_context = EGL_NO_CONTEXT;
    EGLSurface egl_surface = EGL_NO_SURFACE;
};

#endif

render_context_t::render_context_t(mpv_handle *mpv, int width, int height)
    : mpv_(mpv), width_(width), height_(height) {
    if (!initOpenGL()) {
        LOG("Failed to initialize OpenGL context");
        return;
    }

    if (!createFramebuffer()) {
        LOG("Failed to create framebuffer");
        cleanupOpenGL();
        return;
    }

    // Create mpv render context
    mpv_opengl_init_params gl_init_params = {
        .get_proc_address = [](void *ctx, const char *name) -> void* {
#if defined(_WIN32) || defined(_WIN64)
            void *proc = (void*)wglGetProcAddress(name);
            if (!proc) {
                auto *data = static_cast<PlatformGLData*>(ctx);
                if (data && data->opengl32) {
                    proc = (void*)GetProcAddress(data->opengl32, name);
                }
            }
            return proc;
#elif defined(__APPLE__)
            // macOS uses a different approach for GL proc addresses
            return nullptr; // TODO: implement macOS GL proc address
#else
            return (void*)eglGetProcAddress(name);
#endif
        },
        .get_proc_address_ctx = gl_context_,
    };

    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params},
        {MPV_RENDER_PARAM_ADVANCED_CONTROL, &(int){1}},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };

    int result = mpv_render_context_create(&render_ctx_, mpv_, params);
    if (result < 0) {
        LOG("Failed to create mpv render context: %d", result);
        destroyFramebuffer();
        cleanupOpenGL();
        render_ctx_ = nullptr;
        return;
    }

    pixel_buffer_ = new uint8_t[width_ * height_ * 4];
    LOG("MPV render context created: %dx%d", width_, height_);
}

render_context_t::~render_context_t() {
    if (render_ctx_) {
        mpv_render_context_free(render_ctx_);
    }
    destroyFramebuffer();
    cleanupOpenGL();
    delete[] pixel_buffer_;
}

bool render_context_t::initOpenGL() {
#if defined(_WIN32) || defined(_WIN64)
    // Create hidden window for offscreen rendering
    auto *data = new PlatformGLData();

    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "MpvOffscreenGL";
    RegisterClassA(&wc);

    data->hwnd = CreateWindowExA(
        0, "MpvOffscreenGL", "MPV GL",
        0, 0, 0, 1, 1,
        nullptr, nullptr, GetModuleHandleA(nullptr), nullptr
    );

    if (!data->hwnd) {
        delete data;
        return false;
    }

    data->hdc = GetDC(data->hwnd);

    // Set pixel format
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int format = ChoosePixelFormat(data->hdc, &pfd);
    SetPixelFormat(data->hdc, format, &pfd);

    // Create OpenGL context
    data->hglrc = wglCreateContext(data->hdc);
    if (!data->hglrc) {
        ReleaseDC(data->hwnd, data->hdc);
        DestroyWindow(data->hwnd);
        delete data;
        return false;
    }

    wglMakeCurrent(data->hdc, data->hglrc);

    // Load GL extension functions
    data->opengl32 = LoadLibraryA("opengl32.dll");
    loadGLFunctions();

    gl_context_ = data;
    gl_display_ = nullptr;
    return true;

#elif defined(__APPLE__)
    // TODO: implement macOS CGL context
    LOG("macOS OpenGL context not yet implemented");
    return false;

#else
    // Linux: use EGL for offscreen rendering
    auto *data = new PlatformGLData();

    data->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (data->egl_display == EGL_NO_DISPLAY) {
        delete data;
        return false;
    }

    EGLint major, minor;
    if (!eglInitialize(data->egl_display, &major, &minor)) {
        delete data;
        return false;
    }

    EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_NONE
    };

    EGLConfig config;
    EGLint num_configs;
    eglChooseConfig(data->egl_display, config_attribs, &config, 1, &num_configs);
    if (num_configs == 0) {
        eglTerminate(data->egl_display);
        delete data;
        return false;
    }

    // Create pbuffer surface
    EGLint pbuffer_attribs[] = {
        EGL_WIDTH, 1,
        EGL_HEIGHT, 1,
        EGL_NONE
    };
    data->egl_surface = eglCreatePbufferSurface(data->egl_display, config, pbuffer_attribs);

    // Create OpenGL context
    eglBindAPI(EGL_OPENGL_API);
    EGLint context_attribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 3,
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
        EGL_NONE
    };
    data->egl_context = eglCreateContext(data->egl_display, config, EGL_NO_CONTEXT, context_attribs);
    if (data->egl_context == EGL_NO_CONTEXT) {
        eglDestroySurface(data->egl_display, data->egl_surface);
        eglTerminate(data->egl_display);
        delete data;
        return false;
    }

    eglMakeCurrent(data->egl_display, data->egl_surface, data->egl_surface, data->egl_context);

    gl_context_ = data;
    gl_display_ = &data->egl_display;
    return true;
#endif
}

void render_context_t::cleanupOpenGL() {
#if defined(_WIN32) || defined(_WIN64)
    if (gl_context_) {
        auto *data = static_cast<PlatformGLData*>(gl_context_);
        wglMakeCurrent(nullptr, nullptr);
        if (data->hglrc) wglDeleteContext(data->hglrc);
        if (data->hdc && data->hwnd) ReleaseDC(data->hwnd, data->hdc);
        if (data->hwnd) DestroyWindow(data->hwnd);
        if (data->opengl32) FreeLibrary(data->opengl32);
        delete data;
        gl_context_ = nullptr;
    }
#elif !defined(__APPLE__)
    if (gl_context_) {
        auto *data = static_cast<PlatformGLData*>(gl_context_);
        eglMakeCurrent(data->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (data->egl_context != EGL_NO_CONTEXT) eglDestroyContext(data->egl_display, data->egl_context);
        if (data->egl_surface != EGL_NO_SURFACE) eglDestroySurface(data->egl_display, data->egl_surface);
        eglTerminate(data->egl_display);
        delete data;
        gl_context_ = nullptr;
    }
#endif
}

bool render_context_t::createFramebuffer() {
    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_, 0);

    glGenRenderbuffers(1, &rbo_);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width_, height_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo_);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LOG("Framebuffer not complete");
        destroyFramebuffer();
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void render_context_t::destroyFramebuffer() {
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

bool render_context_t::render() {
    if (!render_ctx_ || !fbo_) return false;

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, width_, height_);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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
        LOG("mpv_render_context_render failed: %d", result);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    // Read pixels
    glReadPixels(0, 0, width_, height_, GL_BGRA, GL_UNSIGNED_BYTE, pixel_buffer_);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return true;
}

bool render_context_t::resize(int width, int height) {
    if (width == width_ && height == height_) return true;
    if (width <= 0 || height <= 0) return false;

    width_ = width;
    height_ = height;

    // Recreate framebuffer
    destroyFramebuffer();
    if (!createFramebuffer()) return false;

    // Reallocate pixel buffer
    delete[] pixel_buffer_;
    pixel_buffer_ = new uint8_t[width_ * height_ * 4];

    return true;
}

} // namespace mediampv
