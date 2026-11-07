#include "Win32Surface.h"

#include "D2DRenderer.h"

Win32Surface::Win32Surface() = default;
Win32Surface::~Win32Surface() = default;

void Win32Surface::AttachToWindow(HWND hwnd) {
    hwnd_ = hwnd;
    renderer_ = std::make_unique<D2DRenderer>();
}

void Win32Surface::Detach() {
    if (renderer_) renderer_->Detach();
    renderer_.reset();
    backBuffer_.Reset();
    hwnd_ = nullptr;
    width_ = 0;
    height_ = 0;
}

void Win32Surface::Resize(int width, int height) {
    width_ = width < 0 ? 0 : width;
    height_ = height < 0 ? 0 : height;
    // Buffer is recreated lazily on the next paint (size may match).
}

void Win32Surface::Invalidate(const BBox& box) {
    if (!hwnd_ || !box.IsValid()) return;
    RECT rc{};
    rc.left = static_cast<LONG>(box.left);
    rc.top = static_cast<LONG>(box.top);
    rc.right = static_cast<LONG>(box.right) + 1;  // GDI rects are exclusive
    rc.bottom = static_cast<LONG>(box.bottom) + 1;
    InvalidateRect(hwnd_, &rc, FALSE);
}

void Win32Surface::InvalidateAll() {
    if (!hwnd_) return;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Win32Surface::Paint(HDC hdc, const RECT* invalidRect, const Painter& painter) {
    if (!hdc || width_ <= 0 || height_ <= 0) return;

    if (!backBuffer_.Prepare(hdc, width_, height_)) {
        return;  // allocation failed; keep the previous frame
    }

    HDC mem = backBuffer_.Handle();

    // Single-pass Direct2D rendering into the back buffer.
    if (renderer_ && renderer_->AttachToHdc(mem, width_, height_)) {
        renderer_->clear(background_);
        painter(*renderer_);
        renderer_->Detach();
    }

    // Blit to the target DC.
    backBuffer_.Present(hdc, invalidRect);
}
