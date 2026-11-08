// Win32Surface.h — owns the Direct2D painting surface for the canvas widget:
// double buffering, background clear, invalidation and WM_PAINT handling.
// The Tk layer (CanvasWidget) drives this from the window procedure.
#pragma once

#include <windows.h>
#include <functional>
#include <memory>
#include <vector>

#include "tkd2dcanvas/BBox.h"
#include "tkd2dcanvas/Color.h"

#include "Win32DoubleBuffer.h"

class D2DRenderer;

class Win32Surface {
public:
    // Paints one damaged rectangle (window coordinates)
    using Painter = std::function<void(D2DRenderer&, const RECT&)>;

    Win32Surface();
    ~Win32Surface();

    void AttachToWindow(HWND hwnd);
    void Detach();

    HWND Window() const { return hwnd_; }
    int Width() const { return width_; }
    int Height() const { return height_; }

    void Resize(int width, int height);

    void SetBackground(const Color& color) { background_ = color; }
    const Color& Background() const { return background_; }

    void Invalidate(const BBox& box);   // invalidate a canvas-unit rect
    void InvalidateAll();

    static std::vector<RECT> UpdateRegion(HWND hwnd);
    bool ScrollContent(int dx, int dy);

    void Paint(HDC hdc, const std::vector<RECT>& damaged, const Painter& painter, int destX = 0, int destY = 0);

private:
    HWND hwnd_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    Color background_{255, 255, 255};
    Win32DoubleBuffer backBuffer_;
    std::unique_ptr<D2DRenderer> renderer_;
};
