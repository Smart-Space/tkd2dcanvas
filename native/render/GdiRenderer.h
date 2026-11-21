// GdiRenderer.h — GDI implementation of the Renderer interface.
//
// The renderer draws into a plain GDI device context (the double buffered
// back buffer of the canvas widget).  Edges are smoothed by hand with the
// supersampling engine in GdiAntialias, because GDI itself does not
// anti-alias anything.  Text is drawn with GDI, which anti-aliases TrueType
// glyphs by itself.
#pragma once

#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#include "SurfaceRenderer.h"
#include "GdiAntialias.h"

namespace tkd2dcanvas {

class GdiAntialias;

class GdiRenderer final : public SurfaceRenderer {
public:
    GdiRenderer();
    ~GdiRenderer() override;

    GdiRenderer(const GdiRenderer&) = delete;
    GdiRenderer& operator=(const GdiRenderer&) = delete;

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

    // Binds the renderer to a device context.  `width`/`height` are the
    // pixel size of the surface.
    bool AttachToHdc(void* hdc, int width, int height) override;
    void Detach() override;
    void ResetBinding() override;

    // Number of samples per pixel axis used for the manual anti-aliasing.
    void SetAntiAliasScale(int scale);
    int AntiAliasScale() const;

private:
    // Converts the path points into device coordinates (applying the
    // current translation).  The fractional part is kept: the anti-aliasing
    // engine rasterizes the shape on a scaled buffer, where a half pixel
    // offset decides whether a 1px border covers one pixel or two.
    bool BuildDevicePoints(const Path& path, std::vector<AaPoint>& out) const;

    // Draws the polyline without anti-aliasing (fallback for shapes that
    // are too large for the supersampling buffers).
    void DrawPolylinePlain(const std::vector<AaPoint>& points, bool closed,
                            const PenStyle& pen);

    // Fills an axis aligned rectangle whose edges fall on whole pixels with a
    // plain GDI fill.  Such a rectangle has no anti-aliased edge, so the
    // supersampling engine would produce the same pixels at N*N times the
    // cost.  Returns false when the points do not describe such a rectangle
    // (the caller then uses the anti-aliasing engine).
    bool FillPixelAlignedRect(const std::vector<AaPoint>& points,
                              const Color& color);

    // Creates a GDI pen for the given style.  `widthScale` multiplies the
    // width of the pen (used by the supersampling engine).
    HPEN CreateGdiPen(const PenStyle& pen, COLORREF color, int widthScale) const;

    HDC dc_ = nullptr;
    int width_ = 0;
    int height_ = 0;

    double translateX_ = 0.0;
    double translateY_ = 0.0;
    std::vector<std::pair<double, double>> translateStack_;

    int clipDepth_ = 0;

    // Scratch buffer for the device points of a path.  It is reused between
    // shapes: allocating it per shape would make the C runtime heap grow
    // (it never gives memory back), which shows up as a slowly growing
    // memory usage while scrolling.
    std::vector<AaPoint> devicePoints_;

    // Scratch buffers for the half pixel stroke alignment (see
    // StrokeAlign.h).  Reused, so that drawing a stroke does not allocate.
    Path alignedPath_;
    std::vector<double> alignEdgesX_;
    std::vector<double> alignEdgesY_;

    // Scratch buffer for the plain GDI fallback (the shape could not be
    // anti-aliased).
    std::vector<POINT> roundedPoints_;

    std::unique_ptr<GdiAntialias> antialias_;
};

}  // namespace tkd2dcanvas
