// Win32DoubleBuffer.h — memory-DC back buffer to avoid flicker on WM_PAINT.
#pragma once

#include <windows.h>

class Win32DoubleBuffer {
public:
    Win32DoubleBuffer() = default;
    ~Win32DoubleBuffer() { Reset(); }

    Win32DoubleBuffer(const Win32DoubleBuffer&) = delete;
    Win32DoubleBuffer& operator=(const Win32DoubleBuffer&) = delete;

    // (Re)creates the back buffer for the given size. Returns false on failure.
    bool Prepare(HDC target, int width, int height);

    HDC Handle() const { return memDc_; }
    HBITMAP Bitmap() const { return bitmap_; }
    int Width() const { return width_; }
    int Height() const { return height_; }

    // Blits the back buffer to the target DC over the given rect (or full buffer).
    // (destX, destY) is the position of the back buffer in the target DC.
    bool Present(HDC target, const RECT* rect = nullptr, int destX = 0, int destY = 0) const;

    void Reset();

private:
    HDC memDc_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HBITMAP oldBitmap_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};
