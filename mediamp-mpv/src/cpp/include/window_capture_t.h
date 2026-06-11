#ifndef MEDIAMP_WINDOW_CAPTURE_T_H
#define MEDIAMP_WINDOW_CAPTURE_T_H

#include <cstdint>

#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace mediampv {

class window_capture_t {
public:
    explicit window_capture_t(void *hwnd);
    ~window_capture_t();

    bool capture();
    const uint8_t *getPixels() const { return pixel_buffer_; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

private:
#if defined(_WIN32) || defined(_WIN64)
    HWND hwnd_;
    HDC hdc_window_;
    HDC hdc_mem_;
    HBITMAP hbitmap_;
    HBITMAP old_bitmap_;
#endif
    uint8_t *pixel_buffer_;
    int width_;
    int height_;
};

} // namespace mediampv

#endif // MEDIAMP_WINDOW_CAPTURE_T_H
