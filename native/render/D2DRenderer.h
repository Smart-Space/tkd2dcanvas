// D2DRenderer.h — Direct2D implementation of the Renderer interface.
// Renders directly onto an HDC via ID2D1DCRenderTarget (the supported path
// for binding D2D to a GDI-managed window, e.g. the double-buffered canvas).
// DirectWrite handles text. D2D/DWrite types are kept out of this header
// (the implementation lives in D2DRenderer.cpp).
#pragma once

#include <memory>

#include "D2DTargetKind.h"
#include "SurfaceRenderer.h"

struct ID2D1PathGeometry;
class D2DRendererImpl;

class D2DRenderer final : public SurfaceRenderer {
public:
    // `kind` decides which rasterizer is used (see D2DTargetKind.h).  The
    // render target itself is shared by the whole process (see
    // D2DTargetPool.h).
    explicit D2DRenderer(D2DTargetKind kind = D2DTargetKind::Hardware);
    ~D2DRenderer() override;

    D2DRenderer(const D2DRenderer&) = delete;
    D2DRenderer& operator=(const D2DRenderer&) = delete;

    void clear(const Color& color) override;

    void drawLine(const Path& path, const PenStyle& pen) override;
    void fillPath(const Path& path, const BrushStyle& brush) override;
    void drawPath(const Path& path, const PenStyle& pen, const BrushStyle* brush) override;

    void drawText(const TextLayout& layout, const TextStyle& style) override;
    void drawImage(const ImageRef& image, const Transform2D& transform) override;

    void pushClip(const BBox& box) override;
    void popClip() override;
    void pushTranslate(double dx, double dy) override;
    void popTranslate() override;

    // Binds (creates if needed) a DC render target on the given HDC and opens
    // a drawing frame. Call via Detach() to end the frame. `width`/`height`
    // are the pixel size of the DC surface (required by BindDC). The target
    // keeps its binding between frames when the DC and size are unchanged.
    bool AttachToHdc(void* hdc, int width, int height);
    void Detach();
    void ResetBinding();

private:
    void EnsureBrush();
    void BuildGeometry(const Path& path, ID2D1PathGeometry* geometry) const;

    // The pool (and with it the render target) this renderer draws into.
    D2DTargetKind targetKind_ = D2DTargetKind::Hardware;

    // Scratch buffers for the half pixel stroke alignment (see
    // StrokeAlign.h).  Reused, so that drawing a stroke does not allocate.
    Path alignedPath_;
    std::vector<double> alignEdgesX_;
    std::vector<double> alignEdgesY_;

    std::unique_ptr<D2DRendererImpl> impl_;
};
