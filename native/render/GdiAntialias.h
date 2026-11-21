// GdiAntialias.h — manual anti-aliasing for the GDI back end.
//
// GDI has no anti-aliasing of its own: every path is rasterized with hard,
// aliased edges, so a diagonal line or a circle shows a staircase.  This
// class computes the smooth edges by hand, using nothing but plain GDI:
//
//   1. the shape is drawn with ordinary GDI calls into an off-screen buffer
//      that has "scale" times the resolution of the device,
//   2. that buffer is box-filtered down to device resolution, which yields
//      the exact area that the shape covers in every device pixel,
//   3. the area is used as an alpha value when the shape is composited onto
//      the destination.
//
// No GDI+, no Direct2D, no DirectWrite is involved.
#pragma once

#include <windows.h>

#include <vector>

#include "tkd2dcanvas/Color.h"
#include "tkd2dcanvas/Renderer.h"

namespace tkd2dcanvas {

// A point in device coordinates.
//
// The coordinates keep their fractional part on purpose: the engine
// rasterizes the shape on a buffer that has "scale" times the device
// resolution, where a half pixel offset is meaningful.  Rounding the
// coordinates to whole pixels before that would shift the shape and would
// destroy the half pixel alignment that AlignStrokePath computes (a 1px
// border would cover two pixels with 50% each instead of one solid one).
struct AaPoint {
    double x = 0.0;
    double y = 0.0;
};

class GdiAntialias {
public:
    // Supersampling factors: a factor of N means that every device pixel is
    // sampled N*N times.
    static constexpr int kMinScale = 2;
    static constexpr int kMaxScale = 8;
    static constexpr int kDefaultScale = 4;

    // Limits for a single shape.  Larger shapes fall back to plain GDI so
    // that one huge shape cannot blow up the scratch buffers.
    static constexpr int kMaxDimension = 4096;
    static constexpr long long kMaxSamples = 16LL * 1024 * 1024;

    // Memory budget for the off-screen buffers of one shape.  A shape is
    // rendered in tiles of at most this size, so the buffers never grow
    // beyond the budget - no matter how big the shape is.  (Before the
    // tiling the buffer was sized after the whole shape: a 528x528 shape
    // needed 17 MB at scale 4, and scrolling to a bigger shape made the
    // canvas grow - and stay - bigger.)
    static constexpr long long kScratchBudgetBytes = 1LL * 1024 * 1024;

    // How many tiles a single shape may be split into.  Every tile makes
    // GDI rasterize the shape again, so the count is limited: a shape that
    // would need more tiles is drawn with plain GDI (without anti-aliasing)
    // instead.  The limit is generous - GDI clips the rasterization to the
    // tile, so the extra work is linear in the area of the shape.
    static constexpr int kMaxTiles = 256;

    // How much memory the off-screen buffers may keep between two shapes.
    // A single big shape needs a big buffer, but there is no reason to hold
    // on to it: the buffers would otherwise grow to the biggest shape that
    // was ever drawn and stay that big for the whole life of the widget.
    static constexpr long long kRetainSamples = 4LL * 1024 * 1024;
    static constexpr long long kRetainPixels = 1024LL * 1024;

    GdiAntialias();
    ~GdiAntialias();

    GdiAntialias(const GdiAntialias&) = delete;
    GdiAntialias& operator=(const GdiAntialias&) = delete;

    // Number of samples per pixel axis (clamped to [kMinScale, kMaxScale]).
    void SetScale(int scale);
    int Scale() const { return scale_; }

    // Side length of the tile a shape is rendered in (device pixels).  It
    // follows from the memory budget and the supersampling factor.
    int TileSize() const;

    // Size of the buffers a tile of the given size needs (bytes).
    long long BufferBytes(int tile) const;

    // Overrides the memory budget for the off-screen buffers.  Zero means
    // "the default".  The tests use this to render the same shape with
    // different tile sizes - the pixels must not change.
    void SetScratchBudget(long long bytes);

    // Size of the off-screen buffers the engine currently holds (bytes).
    // This is what the memory of a canvas widget is made of.
    long long ScratchBytes() const;

    // Fills the closed polygon with `color`, with smooth edges.
    bool FillPolygon(HDC dc, const AaPoint* points, int count, const Color& color);

    // Strokes the polyline with `color`, with smooth edges.  A closed path
    // is stroked as a closed path (so the joins at the corners are joins and
    // not caps).
    bool StrokePolyline(HDC dc, const AaPoint* points, int count, bool closed,
                        const Color& color, const PenStyle& pen);

private:
    // Renders the shape into the off-screen buffer, filters it down and
    // composites the result onto `dc`.  `pen` selects a stroke, `brush` a
    // fill; exactly one of them has to be given.
    bool RenderShape(HDC dc, const AaPoint* points, int count, bool closed,
                     const PenStyle* pen, const BrushStyle* brush);

    // Renders the part of the shape that falls into the rectangle
    // (left, top, width, height) - one tile of the shape.  The tile is
    // supersampled on its own, so the scratch buffer only has to hold the
    // tile and not the whole shape.
    //
    // `shapeScaled` holds the vertices of the shape in samples, relative to
    // the shape's bounding box (shapeLeft, shapeTop); the tile's origin
    // (left, top) is relative to the same box.
    bool RenderTile(HDC dc, const AaPoint* points, const POINT* shapeScaled,
                    int count, bool closed, const PenStyle* pen,
                    const BrushStyle* brush, int shapeLeft, int shapeTop,
                    int left, int top, int width, int height);

    // Makes sure the scratch buffer can hold a shape of the given device
    // size (already multiplied by the scale factor).
    bool EnsureScratch(int width, int height);
    // Makes sure the read-back buffer can hold a region of the given size.
    bool EnsureReadBack(int width, int height);
    // Gives back buffers that grew beyond the retention limits.
    void TrimBuffers();
    void ReleaseBuffers();

    // The pen used to stroke a shape in the scratch buffer.  Pens are
    // cached: creating one per shape would churn GDI objects, and GDI's
    // handle table grows with the churn.
    HPEN MaskPen(int widthHi, DWORD capStyle, DWORD joinStyle);

    int scale_ = kDefaultScale;
    long long scratchBudget_ = kScratchBudgetBytes;

    // The brush used to fill a shape in the scratch buffer.  It always has
    // the mask color, so a single brush serves every shape.
    HBRUSH maskBrush_ = nullptr;

    // Cached pens for the scratch buffer.
    struct CachedPen {
        int widthHi;
        DWORD capStyle;
        DWORD joinStyle;
        HPEN pen;
    };
    std::vector<CachedPen> penCache_;

    // Scratch buffers for the point lists.  They are reused between shapes:
    // allocating them per shape would make the C runtime heap grow (it
    // never gives memory back), which shows up as a slowly growing memory
    // usage while scrolling.
    std::vector<POINT> shapeScaled_;
    std::vector<POINT> tileScaled_;

    // Scratch buffer for the dash pattern of a dashed pen.
    std::vector<DWORD> dashes_;

    // Off-screen buffer the shape is drawn into (scale times the device
    // resolution), 32 bit, top-down.
    HDC scratchDC_ = nullptr;
    HBITMAP scratchBitmap_ = nullptr;
    void* scratchBits_ = nullptr;
    int scratchWidth_ = 0;   // in samples
    int scratchHeight_ = 0;

    // Copy of the destination region that is modified (device resolution).
    HDC readBackDC_ = nullptr;
    HBITMAP readBackBitmap_ = nullptr;
    void* readBackBits_ = nullptr;
    int readBackWidth_ = 0;
    int readBackHeight_ = 0;
};

}  // namespace tkd2dcanvas
