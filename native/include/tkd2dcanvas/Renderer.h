// Renderer.h — abstract rendering interface. Implementations (Direct2D, ...)
// live in native/render. Core code never includes platform headers; it only
// talks to this interface.
#pragma once

#include <string>
#include <vector>

#include "tkd2dcanvas/Color.h"
#include "tkd2dcanvas/DashPattern.h"
#include "tkd2dcanvas/Geometry.h"
#include "tkd2dcanvas/BBox.h"
#include "tkd2dcanvas/Transform2D.h"

enum class CapStyle { Flat, Round, Square };
enum class JoinStyle { Miter, Round, Bevel };

struct PenStyle {
    Color color;
    double width = 1.0;
    DashPattern dash;
    CapStyle cap = CapStyle::Flat;
    JoinStyle join = JoinStyle::Miter;
    // True when the stroke ends in real caps. A two point polygon draws a
    // segment with round *joins* at its degenerate vertex (Tk's polygons have
    // no -capstyle option), and such an end is not moved along the segment.
    bool capEnds = true;
    bool antialias = true;
};

struct BrushStyle {
    Color color;
    // Future: hatch brushes, textures. Keep minimal for 0.x.
};

struct TextStyle {
    std::string fontFamily = "TkDefaultFont";
    double size = 12.0;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool overstrike = false;
    Color color;
};

struct TextLayout {
    TextLayout(const std::vector<std::string>& linesIn,
               const std::vector<double>& lineWidthsIn)
        : lines(linesIn), lineWidths(lineWidthsIn) {}

    const std::vector<std::string>& lines;  // already wrapped
    Point origin;                    // anchor point in canvas coords
    std::string anchor = "nw";       // Tk anchor: n/ne/e/se/s/sw/w/nw/c
    double lineSpacing = 0.0;        // 0 = font default
    std::string justify = "left";    // left|center|right; lines are aligned within blockWidth
    double blockWidth = 0.0;         // 0 = widest line at draw time
    double angle = 0.0;              // degrees, counter-clockwise around `origin` (Tk -angle)
    const std::vector<double>& lineWidths;
};

// Vertical metric shared by the text item's bbox and the renderer, so the
// two never disagree on block height.
inline constexpr double kTextLineHeightFactor = 1.25;

inline double TextLineHeight(double fontSize) {
    const double height = fontSize * kTextLineHeightFactor;
    return height > 0.0 ? height : fontSize;
}

// Opaque image handle. Image data is owned by the Tk image subsystem
// (CanvasTkImage) or by the app; the renderer only borrows it.
struct ImageRef {
    const void* handle = nullptr;  // renderer-defined (e.g. ID2D1Bitmap*)
    int width = 0;
    int height = 0;
    const unsigned char* pixels = nullptr;
    int stride = 0;  // bytes per row; 0 = width * 4
    const void* cacheKey = nullptr;
    unsigned generation = 0;
};

class Renderer {
public:
    virtual ~Renderer() = default;

    virtual void clear(const Color& color) = 0;

    virtual void drawLine(const Path& path, const PenStyle& pen) = 0;
    virtual void fillPath(const Path& path, const BrushStyle& brush) = 0;

    // Convenience helpers shared by item drawing code.
    virtual void drawPath(const Path& path, const PenStyle& pen, const BrushStyle* brush) = 0;

    virtual void drawText(const TextLayout& layout, const TextStyle& style) = 0;
    virtual void drawImage(const ImageRef& image, const Transform2D& transform) = 0;

    virtual void pushClip(const BBox& box) = 0;
    virtual void popClip() = 0;

    virtual void pushTranslate(double dx, double dy) = 0;
    virtual void popTranslate() = 0;
};
