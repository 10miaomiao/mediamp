#include "include/window_capture_t.h"
#include "include/log.h"

#if defined(_WIN32) || defined(_WIN64)

namespace mediampv {

window_capture_t::window_capture_t(void *hwnd)
    : hwnd_(static_cast<HWND>(hwnd))
    , hdc_window_(nullptr)
    , hdc_mem_(nullptr)
    , hbitmap_(nullptr)
    , old_bitmap_(nullptr)
    , pixel_buffer_(nullptr)
    , width_(0)
    , height_(0)
{
    if (!hwnd_) {
        LOG("window_capture_t: null hwnd");
        return;
    }

    hdc_window_ = GetDC(hwnd_);
    if (!hdc_window_) {
        LOG("window_capture_t: GetDC failed");
        return;
    }

    hdc_mem_ = CreateCompatibleDC(hdc_window_);
    if (!hdc_mem_) {
        LOG("window_capture_t: CreateCompatibleDC failed");
        ReleaseDC(hwnd_, hdc_window_);
        hdc_window_ = nullptr;
        return;
    }

    LOG("window_capture_t: initialized for hwnd=%p", hwnd_);
}

window_capture_t::~window_capture_t() {
    if (hbitmap_) {
        SelectObject(hdc_mem_, old_bitmap_);
        DeleteObject(hbitmap_);
    }
    if (hdc_mem_) DeleteDC(hdc_mem_);
    if (hdc_window_) ReleaseDC(hwnd_, hdc_window_);
    delete[] pixel_buffer_;
}

bool window_capture_t::capture() {
    if (!hdc_window_ || !hdc_mem_) return false;

    RECT rect;
    if (!GetClientRect(hwnd_, &rect)) return false;

    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;
    if (w <= 0 || h <= 0) return false;

    // Reallocate if size changed
    if (w != width_ || h != height_) {
        if (hbitmap_) {
            SelectObject(hdc_mem_, old_bitmap_);
            DeleteObject(hbitmap_);
            hbitmap_ = nullptr;
        }
        delete[] pixel_buffer_;

        width_ = w;
        height_ = h;
        pixel_buffer_ = new uint8_t[width_ * height_ * 4];

        hbitmap_ = CreateCompatibleBitmap(hdc_window_, width_, height_);
        if (!hbitmap_) {
            LOG("window_capture_t: CreateCompatibleBitmap failed");
            return false;
        }
        old_bitmap_ = (HBITMAP)SelectObject(hdc_mem_, hbitmap_);
    }

    // Copy window content to memory DC
    if (!BitBlt(hdc_mem_, 0, 0, width_, height_, hdc_window_, 0, 0, SRCCOPY)) {
        LOG("window_capture_t: BitBlt failed");
        return false;
    }

    // Read pixels from bitmap
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width_;
    bmi.bmiHeader.biHeight = -height_; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    if (!GetDIBits(hdc_mem_, hbitmap_, 0, height_, pixel_buffer_, &bmi, DIB_RGB_COLORS)) {
        LOG("window_capture_t: GetDIBits failed");
        return false;
    }

    return true;
}

} // namespace mediampv

#else

// Non-Windows stub
namespace mediampv {

window_capture_t::window_capture_t(void *hwnd)
    : pixel_buffer_(nullptr), width_(0), height_(0) {}

window_capture_t::~window_capture_t() {
    delete[] pixel_buffer_;
}

bool window_capture_t::capture() { return false; }

} // namespace mediampv

#endif
