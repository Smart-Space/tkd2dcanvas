#include "Win32DoubleBuffer.h"

#include <cstdlib>

bool Win32DoubleBuffer::Prepare(HDC target, int width, int height) {
    if (!target || width <= 0 || height <= 0) return false;

    if (memDc_ && width_ == width && height_ == height) return true;
    Reset();

    memDc_ = CreateCompatibleDC(target);
    if (!memDc_) return false;

    // ID2D1DCRenderTarget::BindDC requires a 32bpp DIB section (top-down),
    // not a device-dependent bitmap.
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    bitmap_ = CreateDIBSection(target, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap_) {
        Reset();
        return false;
    }

    oldBitmap_ = static_cast<HBITMAP>(SelectObject(memDc_, bitmap_));
    width_ = width;
    height_ = height;
    return true;
}

bool Win32DoubleBuffer::Present(HDC target, const RECT* rect, int destX, int destY) const {
    if (!target || !memDc_) return false;

    RECT full{0, 0, width_, height_};
    const RECT& r = rect ? *rect : full;
    const int w = r.right - r.left;
    const int h = r.bottom - r.top;
    if (w <= 0 || h <= 0) return false;

    return BitBlt(target, destX + r.left, destY + r.top, w, h,
                  memDc_, r.left, r.top, SRCCOPY) == TRUE;
}

bool Win32DoubleBuffer::ScrollContent(int dx, int dy) {
    if (!memDc_ || width_ <= 0 || height_ <= 0) return false;
    if (dx == 0 && dy == 0) return true;
    if (std::abs(dx) >= width_ || std::abs(dy) >= height_) return false;

    const int srcX = dx > 0 ? 0 : -dx;
    const int dstX = dx > 0 ? dx : 0;
    const int srcY = dy > 0 ? 0 : -dy;
    const int dstY = dy > 0 ? dy : 0;
    const int w = width_ - std::abs(dx);
    const int h = height_ - std::abs(dy);

    return BitBlt(memDc_, dstX, dstY, w, h, memDc_, srcX, srcY, SRCCOPY) == TRUE;
}

void Win32DoubleBuffer::Reset() {
    if (memDc_) {
        if (oldBitmap_) SelectObject(memDc_, oldBitmap_);
        DeleteDC(memDc_);
        memDc_ = nullptr;
    }
    if (bitmap_) {
        DeleteObject(bitmap_);
        bitmap_ = nullptr;
    }
    oldBitmap_ = nullptr;
    width_ = 0;
    height_ = 0;
}
