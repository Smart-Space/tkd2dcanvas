// GdiScratchPool.cpp — the process-wide off-screen buffers of the
// anti-aliasing engine.
#include "GdiScratchPool.h"

#include <algorithm>
#include <cstring>

namespace tkd2dcanvas {

namespace {

// Creates a top-down 32 bit DIB section whose pixels can be accessed
// directly through a pointer.
HBITMAP CreateTopDownDib(HDC dc, int width, int height, void** bits) {
    BITMAPINFO info;
    std::memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;  // top-down
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    *bits = nullptr;
    return CreateDIBSection(dc, &info, DIB_RGB_COLORS, bits, nullptr, 0);
}

}  // namespace

GdiScratchPool& GdiScratchPool::Instance() {
    // A function local static: it is created on first use and destroyed
    // when the module goes away (the GDI objects are released then).
    static GdiScratchPool pool;
    return pool;
}

GdiScratchPool::GdiScratchPool() {
    InitializeCriticalSection(&lock_);
}

GdiScratchPool::~GdiScratchPool() {
    ReleaseBuffers();
    DeleteCriticalSection(&lock_);
}

void GdiScratchPool::ReleaseBuffers() {
    if (maskBrush_) {
        DeleteObject(maskBrush_);
        maskBrush_ = nullptr;
    }
    for (CachedPen& entry : penCache_) {
        if (entry.pen) DeleteObject(entry.pen);
    }
    penCache_.clear();

    if (scratchDC_) {
        if (scratchBitmap_) SelectObject(scratchDC_, scratchBitmap_);
        DeleteDC(scratchDC_);
        scratchDC_ = nullptr;
    }
    if (scratchBitmap_) {
        DeleteObject(scratchBitmap_);
        scratchBitmap_ = nullptr;
    }
    scratchBits_ = nullptr;
    scratchWidth_ = 0;
    scratchHeight_ = 0;

    if (readBackDC_) {
        if (readBackBitmap_) SelectObject(readBackDC_, readBackBitmap_);
        DeleteDC(readBackDC_);
        readBackDC_ = nullptr;
    }
    if (readBackBitmap_) {
        DeleteObject(readBackBitmap_);
        readBackBitmap_ = nullptr;
    }
    readBackBits_ = nullptr;
    readBackWidth_ = 0;
    readBackHeight_ = 0;
}

bool GdiScratchPool::EnsureScratch(int width, int height) {
    if (!scratchDC_) {
        scratchDC_ = CreateCompatibleDC(nullptr);
        if (!scratchDC_) return false;
    }
    // The buffer is shared: it is only grown, never shrunk, so that a
    // sequence of shapes of similar size does not allocate over and over.
    if (width <= scratchWidth_ && height <= scratchHeight_) return true;

    // Round the size up so that shapes of slightly different sizes can
    // share the same buffer.
    const int newWidth = std::max((width + 63) & ~63, scratchWidth_);
    const int newHeight = std::max((height + 63) & ~63, scratchHeight_);

    void* bits = nullptr;
    HBITMAP bitmap = CreateTopDownDib(scratchDC_, newWidth, newHeight, &bits);
    if (!bitmap) return false;
    if (scratchBitmap_) {
        SelectObject(scratchDC_, scratchBitmap_);
        DeleteObject(scratchBitmap_);
    }
    SelectObject(scratchDC_, bitmap);
    scratchBitmap_ = bitmap;
    scratchBits_ = bits;
    scratchWidth_ = newWidth;
    scratchHeight_ = newHeight;
    return true;
}

bool GdiScratchPool::EnsureReadBack(int width, int height) {
    if (!readBackDC_) {
        readBackDC_ = CreateCompatibleDC(nullptr);
        if (!readBackDC_) return false;
    }
    if (width <= readBackWidth_ && height <= readBackHeight_) return true;

    const int newWidth = std::max((width + 63) & ~63, readBackWidth_);
    const int newHeight = std::max((height + 63) & ~63, readBackHeight_);

    void* bits = nullptr;
    HBITMAP bitmap = CreateTopDownDib(readBackDC_, newWidth, newHeight, &bits);
    if (!bitmap) return false;
    if (readBackBitmap_) {
        SelectObject(readBackDC_, readBackBitmap_);
        DeleteObject(readBackBitmap_);
    }
    SelectObject(readBackDC_, bitmap);
    readBackBitmap_ = bitmap;
    readBackBits_ = bits;
    readBackWidth_ = newWidth;
    readBackHeight_ = newHeight;
    return true;
}

void GdiScratchPool::Trim() {
    // Buffers that grew beyond the retention limit are given back: they were
    // only needed for one unusually big shape.
    if (scratchWidth_ > 0 &&
        static_cast<long long>(scratchWidth_) * scratchHeight_ > kRetainSamples) {
        if (scratchBitmap_) {
            SelectObject(scratchDC_, scratchBitmap_);
            DeleteObject(scratchBitmap_);
            scratchBitmap_ = nullptr;
        }
        scratchBits_ = nullptr;
        scratchWidth_ = 0;
        scratchHeight_ = 0;
    }
    if (readBackWidth_ > 0 &&
        static_cast<long long>(readBackWidth_) * readBackHeight_ > kRetainPixels) {
        if (readBackBitmap_) {
            SelectObject(readBackDC_, readBackBitmap_);
            DeleteObject(readBackBitmap_);
            readBackBitmap_ = nullptr;
        }
        readBackBits_ = nullptr;
        readBackWidth_ = 0;
        readBackHeight_ = 0;
    }
}

HPEN GdiScratchPool::MaskPen(int widthHi, DWORD capStyle, DWORD joinStyle) {
    for (const CachedPen& entry : penCache_) {
        if (entry.widthHi == widthHi && entry.capStyle == capStyle &&
            entry.joinStyle == joinStyle) {
            return entry.pen;
        }
    }

    LOGBRUSH logBrush;
    logBrush.lbStyle = BS_SOLID;
    logBrush.lbColor = kAaMaskColor;
    logBrush.lbHatch = 0;
    const DWORD style = PS_GEOMETRIC | capStyle | joinStyle;
    HPEN pen = ExtCreatePen(style, static_cast<DWORD>(widthHi), &logBrush, 0, nullptr);
    if (!pen) return nullptr;

    penCache_.push_back(CachedPen{widthHi, capStyle, joinStyle, pen});
    return pen;
}

HBRUSH GdiScratchPool::MaskBrush() {
    if (!maskBrush_) {
        maskBrush_ = CreateSolidBrush(kAaMaskColor);
    }
    return maskBrush_;
}

long long GdiScratchPool::Bytes() const {
    const long long scratch =
        static_cast<long long>(scratchWidth_) * scratchHeight_ * 4;
    const long long readBack =
        static_cast<long long>(readBackWidth_) * readBackHeight_ * 4;
    return scratch + readBack;
}

int GdiScratchPool::BufferCount() const {
    return (scratchBitmap_ ? 1 : 0) + (readBackBitmap_ ? 1 : 0);
}

}  // namespace tkd2dcanvas
