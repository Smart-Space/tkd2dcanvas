// GdiRenderer.cpp 鈥?GDI implementation of the Renderer interface.
#include "GdiRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "GdiAntialias.h"
#include "Utf16.h"
#include "tkd2dcanvas/Anchor.h"
#include "tkd2dcanvas/StrokeAlign.h"

namespace tkd2dcanvas {

namespace {

inline COLORREF ToColorRef(const Color& c) {
    return RGB(c.r, c.g, c.b);
}

// Creates a GDI font for the given style.
//
// The size of a text style is in device independent pixels (DIP, i.e.
// 1/96 inch), which is what DirectWrite uses as well, so the same value is
// used here: the font height is the size in pixels.  (Converting points to
// pixels would make the GDI text 96/72 times bigger than the Direct2D one.)
HFONT CreateGdiFont(const TextStyle& style) {
    const int height = -static_cast<int>(std::lround(style.size));
    const std::wstring family = Utf8ToWide(style.fontFamily.empty() ? "Segoe UI"
                                                                   : style.fontFamily);
    return CreateFontW(height, 0, 0, 0, style.bold ? FW_BOLD : FW_NORMAL,
                       style.italic ? TRUE : FALSE,
                       style.underline ? TRUE : FALSE,
                       style.overstrike ? TRUE : FALSE,
                       DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                       family.c_str());
}

}  // namespace

GdiRenderer::GdiRenderer() : antialias_(std::make_unique<GdiAntialias>()) {}

GdiRenderer::~GdiRenderer() = default;

bool GdiRenderer::AttachToHdc(void* hdc, int width, int height) {
    if (!hdc) return false;
    dc_ = static_cast<HDC>(hdc);
    width_ = width;
    height_ = height;
    return true;
}

void GdiRenderer::Detach() {
    dc_ = nullptr;
    width_ = 0;
    height_ = 0;
    clipDepth_ = 0;
    translateX_ = 0.0;
    translateY_ = 0.0;
    translateStack_.clear();
}

void GdiRenderer::ResetBinding() {
    Detach();
}

void GdiRenderer::SetAntiAliasScale(int scale) {
    antialias_->SetScale(scale);
}

int GdiRenderer::AntiAliasScale() const {
    return antialias_->Scale();
}

// ---------------------------------------------------------------------------
// Clipping and translation
// ---------------------------------------------------------------------------

void GdiRenderer::pushClip(const BBox& box) {
    if (!dc_) return;
    const RECT rect{static_cast<LONG>(std::floor(box.left)),
                    static_cast<LONG>(std::floor(box.top)),
                    static_cast<LONG>(std::ceil(box.right)),
                    static_cast<LONG>(std::ceil(box.bottom))};
    IntersectClipRect(dc_, rect.left, rect.top, rect.right, rect.bottom);
    ++clipDepth_;
}

void GdiRenderer::popClip() {
    if (!dc_ || clipDepth_ <= 0) return;
    // GDI has no clip stack: the clip is intersected with the previous one
    // on push and restored by saving/restoring the DC state around the
    // paint pass.  The canvas paints each damage rectangle as its own
    // clipped pass, so restoring the clip to "no clip" is enough here.
    SelectClipRgn(dc_, nullptr);
    --clipDepth_;
}

void GdiRenderer::pushTranslate(double dx, double dy) {
    translateStack_.emplace_back(translateX_, translateY_);
    translateX_ += dx;
    translateY_ += dy;
}

void GdiRenderer::popTranslate() {
    if (translateStack_.empty()) return;
    const auto saved = translateStack_.back();
    translateStack_.pop_back();
    translateX_ = saved.first;
    translateY_ = saved.second;
}

// ---------------------------------------------------------------------------
// Drawing primitives
// ---------------------------------------------------------------------------

void GdiRenderer::clear(const Color& color) {
    if (!dc_) return;
    RECT rect{0, 0, width_, height_};
    HBRUSH brush = CreateSolidBrush(ToColorRef(color));
    FillRect(dc_, &rect, brush);
    DeleteObject(brush);
}

bool GdiRenderer::BuildDevicePoints(const Path& path,
                                    std::vector<AaPoint>& out) const {
    if (path.points.empty()) return false;
    out.clear();
    out.reserve(path.points.size());
    for (const Point& p : path.points) {
        AaPoint device;
        device.x = p.x + translateX_;
        device.y = p.y + translateY_;
        out.push_back(device);
    }
    return true;
}

void GdiRenderer::drawLine(const Path& path, const PenStyle& pen) {
    if (!dc_ || path.Size() < 2) return;

    // A zero width outline is not drawn at all (this is what Tk does and what
    // the Direct2D back end does as well: Direct2D rejects a stroke width of
    // zero).  Clamping it to one pixel would draw a border that is not there.
    if (pen.width <= 0.0) return;

    // Half-pixel stroke edges are moved onto the pixel grid (see
    // AlignStrokePath), so that a 1px border stays 1px wide and crisp.
    Path aligned;
    const Path& effective =
        tkd2dcanvas::AlignStrokePath(path, pen.width, pen.cap, pen.capEnds, aligned)
            ? aligned
            : path;

    std::vector<AaPoint> points;
    if (!BuildDevicePoints(effective, points)) return;

    if (!antialias_->StrokePolyline(dc_, points.data(),
                                    static_cast<int>(points.size()),
                                    effective.closed, pen.color, pen)) {
        // Fall back to plain GDI when the shape cannot be anti-aliased.
        DrawPolylinePlain(points, effective.closed, pen);
    }
}

void GdiRenderer::DrawPolylinePlain(const std::vector<AaPoint>& points,
                                     bool closed, const PenStyle& pen) {
    std::vector<POINT> rounded(points.size());
    for (std::size_t i = 0; i < points.size(); ++i) {
        rounded[i].x = static_cast<LONG>(std::lround(points[i].x));
        rounded[i].y = static_cast<LONG>(std::lround(points[i].y));
    }

    HPEN handle = CreateGdiPen(pen, ToColorRef(pen.color), 1);
    if (!handle) return;
    HGDIOBJ oldPen = SelectObject(dc_, handle);
    SetBkMode(dc_, TRANSPARENT);
    if (closed) {
        BeginPath(dc_);
        Polyline(dc_, rounded.data(), static_cast<int>(rounded.size()));
        CloseFigure(dc_);
        EndPath(dc_);
        StrokePath(dc_);
    } else {
        Polyline(dc_, rounded.data(), static_cast<int>(rounded.size()));
    }
    SelectObject(dc_, oldPen);
    DeleteObject(handle);
}

HPEN GdiRenderer::CreateGdiPen(const PenStyle& pen, COLORREF color,
                               int widthScale) const {
    DWORD style = PS_GEOMETRIC;
    switch (pen.cap) {
        case CapStyle::Round:
            style |= PS_ENDCAP_ROUND;
            break;
        case CapStyle::Square:
            style |= PS_ENDCAP_SQUARE;
            break;
        case CapStyle::Flat:
        default:
            style |= PS_ENDCAP_FLAT;
            break;
    }
    switch (pen.join) {
        case JoinStyle::Round:
            style |= PS_JOIN_ROUND;
            break;
        case JoinStyle::Bevel:
            style |= PS_JOIN_BEVEL;
            break;
        case JoinStyle::Miter:
        default:
            style |= PS_JOIN_MITER;
            break;
    }

    int width = std::max(1, static_cast<int>(std::lround(pen.width))) * widthScale;

    std::vector<DWORD> dashes;
    if (!pen.dash.IsSolid()) {
        for (float dash : pen.dash.dashes) {
            float scaled = dash * widthScale;
            if (scaled < 1.0f) scaled = 1.0f;
            dashes.push_back(static_cast<DWORD>(std::lround(scaled)));
        }
        if (dashes.size() % 2 != 0) {
            dashes.push_back(dashes.back());
        }
        if (dashes.size() >= 2) {
            style |= PS_DASH;
        } else {
            dashes.clear();
        }
    }

    LOGBRUSH logBrush;
    logBrush.lbStyle = BS_SOLID;
    logBrush.lbColor = color;
    logBrush.lbHatch = 0;
    return ExtCreatePen(style, static_cast<DWORD>(width), &logBrush,
                        static_cast<DWORD>(dashes.size()),
                        dashes.empty() ? nullptr : dashes.data());
}

void GdiRenderer::fillPath(const Path& path, const BrushStyle& brush) {
    if (!dc_ || path.Size() < 3) return;

    std::vector<AaPoint> points;
    if (!BuildDevicePoints(path, points)) return;

    // Fast path: an axis aligned rectangle whose edges fall on whole pixels
    // has no anti-aliased edge at all -- every pixel is either fully covered
    // or not covered.  The supersampling engine would reproduce that exactly,
    // but it costs N*N times as much (a full screen background goes from
    // 4.0 ms to 0.016 ms).  The canvas background is exactly such a
    // rectangle, and it is filled on every paint, so this matters.
    if (FillPixelAlignedRect(points, brush.color)) return;

    if (!antialias_->FillPolygon(dc_, points.data(),
                                 static_cast<int>(points.size()), brush.color)) {
        // Fall back to plain GDI when the shape cannot be anti-aliased.
        std::vector<POINT> rounded(points.size());
        for (std::size_t i = 0; i < points.size(); ++i) {
            rounded[i].x = static_cast<LONG>(std::lround(points[i].x));
            rounded[i].y = static_cast<LONG>(std::lround(points[i].y));
        }
        HBRUSH handle = CreateSolidBrush(ToColorRef(brush.color));
        HGDIOBJ oldBrush = SelectObject(dc_, handle);
        HGDIOBJ oldPen = SelectObject(dc_, GetStockObject(NULL_PEN));
        SetPolyFillMode(dc_, ALTERNATE);
        Polygon(dc_, rounded.data(), static_cast<int>(rounded.size()));
        SelectObject(dc_, oldPen);
        SelectObject(dc_, oldBrush);
        DeleteObject(handle);
    }
}

bool GdiRenderer::FillPixelAlignedRect(const std::vector<AaPoint>& points,
                                       const Color& color) {
    if (points.size() != 4) return false;

    // The four points have to form an axis aligned rectangle.
    const AaPoint& a = points[0];
    const AaPoint& b = points[1];
    const AaPoint& c = points[2];
    const AaPoint& d = points[3];
    if (a.y != b.y || b.x != c.x || c.y != d.y || d.x != a.x) return false;

    // Every coordinate has to be a whole pixel.
    const double xs[4] = {a.x, b.x, c.x, d.x};
    const double ys[4] = {a.y, b.y, c.y, d.y};
    for (int i = 0; i < 4; ++i) {
        if (xs[i] != std::floor(xs[i]) || ys[i] != std::floor(ys[i])) return false;
    }

    const LONG left = static_cast<LONG>(std::min(a.x, b.x));
    const LONG top = static_cast<LONG>(std::min(a.y, c.y));
    const LONG right = static_cast<LONG>(std::max(a.x, b.x));
    const LONG bottom = static_cast<LONG>(std::max(a.y, c.y));
    if (right <= left || bottom <= top) return false;

    RECT rect{left, top, right, bottom};
    HBRUSH brush = CreateSolidBrush(ToColorRef(color));
    FillRect(dc_, &rect, brush);
    DeleteObject(brush);
    return true;
}

void GdiRenderer::drawPath(const Path& path, const PenStyle& pen,
                           const BrushStyle* brush) {
    if (brush) fillPath(path, *brush);
    drawLine(path, pen);
}

// ---------------------------------------------------------------------------
// Text
// ---------------------------------------------------------------------------

void GdiRenderer::drawText(const TextLayout& layout, const TextStyle& style) {
    if (!dc_ || layout.lines.empty()) return;

    const double size = style.size > 0.0 ? style.size : 12.0;
    HFONT font = CreateGdiFont(style);
    if (!font) return;
    HGDIOBJ oldFont = SelectObject(dc_, font);
    SetBkMode(dc_, TRANSPARENT);
    SetTextColor(dc_, ToColorRef(style.color));

    const double lineHeight = layout.lineSpacing > 0.0
        ? layout.lineSpacing
        : TextLineHeight(size);
    const double blockHeight = lineHeight * static_cast<double>(layout.lines.size());

    // Measure the lines.
    std::vector<double> lineWidths;
    lineWidths.reserve(layout.lines.size());
    double measuredBlockWidth = 0.0;
    if (layout.lineWidths.size() == layout.lines.size()) {
        lineWidths = layout.lineWidths;
        for (const double w : lineWidths) {
            measuredBlockWidth = std::max(measuredBlockWidth, w);
        }
    } else {
        for (const std::string& line : layout.lines) {
            const std::wstring wideLine = Utf8ToWide(line);
            double lineWidth = 0.0;
            if (!wideLine.empty()) {
                SIZE extent{};
                if (GetTextExtentPoint32W(dc_, wideLine.c_str(),
                                          static_cast<int>(wideLine.size()),
                                          &extent)) {
                    lineWidth = static_cast<double>(extent.cx);
                }
            }
            lineWidths.push_back(lineWidth);
            measuredBlockWidth = std::max(measuredBlockWidth, lineWidth);
        }
    }
    const double blockWidth = layout.blockWidth > 0.0 ? layout.blockWidth : measuredBlockWidth;

    const std::string justify = layout.justify.empty() ? "left" : layout.justify;
    const std::string anchor = layout.anchor.empty() ? "nw" : layout.anchor;

    const double originX = layout.origin.x + translateX_;
    const double originY = layout.origin.y + translateY_;
    Point topLeft = AnchorTopLeft(anchor, originX, originY, blockWidth, blockHeight);

    const double angle = layout.angle;
    // The world transform is part of the DC state: it has to be restored
    // afterwards, otherwise every later drawing operation (including the
    // blit of the back buffer onto the screen) would be transformed too.
    const int savedDc = angle != 0.0 ? SaveDC(dc_) : 0;
    if (angle != 0.0) {
        // Rotate around the anchor point, like the Direct2D backend does.
        const double radians = angle * (3.14159265358979323846 / 180.0);
        const double cosA = std::cos(radians);
        const double sinA = std::sin(radians);
        const double offsetX = AnchorOffsetX(anchor, blockWidth);
        const double offsetY = AnchorOffsetY(anchor, blockHeight);
        const double ox = originX + offsetX;
        const double oy = originY + offsetY;

        SetGraphicsMode(dc_, GM_ADVANCED);
        XFORM xform{};
        xform.eM11 = static_cast<FLOAT>(cosA);
        xform.eM12 = static_cast<FLOAT>(-sinA);
        xform.eM21 = static_cast<FLOAT>(sinA);
        xform.eM22 = static_cast<FLOAT>(cosA);
        xform.eDx = static_cast<FLOAT>(ox);
        xform.eDy = static_cast<FLOAT>(oy);
        SetWorldTransform(dc_, &xform);

        // The text is drawn relative to the anchor point now.
        topLeft = Point{topLeft.x - ox, topLeft.y - oy};
    }

    int lineIndex = 0;
    for (const std::string& line : layout.lines) {
        const std::wstring wideLine = Utf8ToWide(line);
        if (!wideLine.empty()) {
            const double x = topLeft.x +
                JustifyOffset(justify, blockWidth, lineWidths[static_cast<size_t>(lineIndex)]);
            const double y = topLeft.y + lineHeight * lineIndex;
            ExtTextOutW(dc_, static_cast<int>(std::lround(x)),
                        static_cast<int>(std::lround(y)), 0, nullptr,
                        wideLine.c_str(), static_cast<int>(wideLine.size()), nullptr);
        }
        ++lineIndex;
    }

    if (angle != 0.0) {
        RestoreDC(dc_, savedDc);
    }

    SelectObject(dc_, oldFont);
    DeleteObject(font);
}

// ---------------------------------------------------------------------------
// Images
// ---------------------------------------------------------------------------

void GdiRenderer::drawImage(const ImageRef& image, const Transform2D& transform) {
    if (!dc_ || image.width <= 0 || image.height <= 0) return;

    if (!image.pixels) {
        // An opaque image handle (e.g. an XBM) is not supported by the GDI
        // back end yet.
        return;
    }

    const int stride = image.stride > 0 ? image.stride : image.width * 4;

    // Build a 32 bit DIB (B, G, R, A) with premultiplied alpha, which is
    // what GDI's AlphaBlend expects.
    BITMAPINFO info;
    std::memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = image.width;
    info.bmiHeader.biHeight = -image.height;  // top-down
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    HDC memDC = CreateCompatibleDC(dc_);
    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(memDC, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap) {
        DeleteDC(memDC);
        return;
    }
    HGDIOBJ oldBitmap = SelectObject(memDC, bitmap);

    auto* dest = static_cast<unsigned char*>(bits);
    for (int y = 0; y < image.height; ++y) {
        const unsigned char* src = image.pixels + static_cast<size_t>(y) * stride;
        unsigned char* row = dest + static_cast<size_t>(y) * image.width * 4;
        for (int x = 0; x < image.width; ++x) {
            const unsigned char a = src[x * 4 + 3];
            row[x * 4 + 0] = static_cast<unsigned char>((src[x * 4 + 2] * a + 127) / 255);
            row[x * 4 + 1] = static_cast<unsigned char>((src[x * 4 + 1] * a + 127) / 255);
            row[x * 4 + 2] = static_cast<unsigned char>((src[x * 4 + 0] * a + 127) / 255);
            row[x * 4 + 3] = a;
        }
    }

    double a, b, c, d, e, f;
    transform.Coefficients(a, b, c, d, e, f);

    // The source bitmap is a 32 bit DIB with *premultiplied* alpha, so
    // AlphaFormat has to be AC_SRC_ALPHA - any other value makes AlphaBlend
    // fail with ERROR_INVALID_PARAMETER.
    const BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};

    if (a == 1.0 && b == 0.0 && c == 0.0 && d == 1.0) {
        // A plain translation (what the canvas uses for images): the image is
        // blitted at its natural size.  AlphaBlend fails with
        // ERROR_INVALID_PARAMETER when the destination rectangle reaches out
        // of the device, so the rectangle is clipped against the surface -
        // and the source rectangle is clipped with it.
        int destLeft = static_cast<int>(std::lround(e + translateX_));
        int destTop = static_cast<int>(std::lround(f + translateY_));
        int destRight = destLeft + image.width;
        int destBottom = destTop + image.height;

        int srcLeft = 0;
        int srcTop = 0;
        int srcRight = image.width;
        int srcBottom = image.height;

        if (destLeft < 0) {
            srcLeft = -destLeft;
            destLeft = 0;
        }
        if (destTop < 0) {
            srcTop = -destTop;
            destTop = 0;
        }
        if (destRight > width_) {
            srcRight -= destRight - width_;
            destRight = width_;
        }
        if (destBottom > height_) {
            srcBottom -= destBottom - height_;
            destBottom = height_;
        }

        const int destWidth = destRight - destLeft;
        const int destHeight = destBottom - destTop;
        if (destWidth > 0 && destHeight > 0) {
            AlphaBlend(dc_, destLeft, destTop, destWidth, destHeight, memDC,
                       srcLeft, srcTop, destWidth, destHeight, blend);
        }
    } else {
        // A general transform (scale or rotation): the world transform does
        // the work.  The state is saved and restored, because the world
        // transform would otherwise affect every later drawing operation.
        const int savedDc = SaveDC(dc_);
        SetGraphicsMode(dc_, GM_ADVANCED);
        XFORM xform{};
        xform.eM11 = static_cast<FLOAT>(a);
        xform.eM12 = static_cast<FLOAT>(b);
        xform.eM21 = static_cast<FLOAT>(c);
        xform.eM22 = static_cast<FLOAT>(d);
        xform.eDx = static_cast<FLOAT>(e + translateX_);
        xform.eDy = static_cast<FLOAT>(f + translateY_);
        SetWorldTransform(dc_, &xform);

        AlphaBlend(dc_, 0, 0, image.width, image.height, memDC, 0, 0,
                   image.width, image.height, blend);

        RestoreDC(dc_, savedDc);
    }

    SelectObject(memDC, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memDC);
}

}  // namespace tkd2dcanvas
