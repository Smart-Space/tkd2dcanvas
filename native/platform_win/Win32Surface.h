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
#include "tkd2dcanvas/Renderer.h"

#include "Win32DoubleBuffer.h"
#include "SurfaceRenderer.h"

// Which renderer paints the canvas.
enum class RendererKind {
    Direct2D,  // Direct2D/DirectWrite (the default)
    Gdi,       // plain GDI with manual anti-aliasing
};

class Win32Surface {
public:
    // Paints one damaged rectangle (window coordinates)
    using Painter = std::function<void(Renderer&, const RECT&)>;

    Win32Surface();
    ~Win32Surface();

    void AttachToWindow(HWND hwnd);
    void Detach();

    // Chooses the renderer. Has to be called before AttachToWindow().
    void SetRendererKind(RendererKind kind) { rendererKind_ = kind; }
    RendererKind GetRendererKind() const { return rendererKind_; }

    // Number of samples per pixel axis used by the manual anti-aliasing of
    // the GDI renderer (ignored by the other renderers).
    void SetAntiAliasScale(int scale);

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
    RendererKind rendererKind_ = RendererKind::Direct2D;
    int aaScale_ = 4;
    std::unique_ptr<SurfaceRenderer> renderer_;
};
