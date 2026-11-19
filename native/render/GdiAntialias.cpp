// GdiAntialias.cpp — implementation of the manual anti-aliasing engine.
#include "GdiAntialias.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace tkd2dcanvas {

namespace {

// The color the shape is drawn with inside the off-screen buffer.  The
// engine compares the samples against it to find out which ones are
// covered, so it only has to differ from the color the buffer is cleared
// with.
constexpr COLORREF kMaskColor = RGB(1, 2, 3);
constexpr COLORREF kClearColor = RGB(0, 0, 0);

// A COLORREF is 0x00BBGGRR, while a DWORD read from a 32 bit DIB is
// 0xAARRGGBB (the DIB stores the pixels as B, G, R, X).  The two have to be
// converted into each other whenever a color is compared with a pixel or
// written into a pixel.
inline DWORD DwordFromColor(COLORREF color) {
    return static_cast<DWORD>(((color >> 16) & 0xFF) |
                               static_cast<DWORD>(color & 0xFF00) |
                               (static_cast<DWORD>(color & 0xFF) << 16));
}

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

// GDI caps and joins, mapped from the renderer's style enums.
DWORD GdiCapStyle(CapStyle cap) {
    switch (cap) {
        case CapStyle::Round:
            return PS_ENDCAP_ROUND;
        case CapStyle::Square:
            return PS_ENDCAP_SQUARE;
        case CapStyle::Flat:
        default:
            return PS_ENDCAP_FLAT;
    }
}

// Writes one half-disk (a round cap) into the mask buffer.
//
// GDI rasterizes PS_ENDCAP_ROUND with aliased chords: the two halves of the
// cap cover different pixels (asymmetric) and the tip tangent can fall on a
// pixel boundary, so the cap looks flat next to the crisp straight edge the
// alignment produces. Writing the samples directly makes the cap perfectly
// symmetric and smoothly anti-aliased, and keeps it consistent with the
// rest of the engine (the same hi-res mask + box filter is used).
//
// center/outward are in device pixels; radiusHi is the cap radius in
// hi-res samples (the pen width is an integer number of device pixels,
// multiplied by the scale factor).
void DrawRoundCapEnd(DWORD* bits, int scratchWidth, int scale,
                     double centerX, double centerY,
                     double outwardX, double outwardY,
                     double radiusHi,
                     int left, int top, int hiWidth, int hiHeight) {
    const double cx = (centerX - left) * scale;
    const double cy = (centerY - top) * scale;
    const double r2 = radiusHi * radiusHi;
    const int x0 = std::max(0, static_cast<int>(std::floor(cx - radiusHi)));
    const int x1 = std::min(hiWidth - 1, static_cast<int>(std::floor(cx + radiusHi)));
    const int y0 = std::max(0, static_cast<int>(std::floor(cy - radiusHi)));
    const int y1 = std::min(hiHeight - 1, static_cast<int>(std::floor(cy + radiusHi)));
    const DWORD mask = DwordFromColor(kMaskColor);
    for (int y = y0; y <= y1; ++y) {
        DWORD* row = bits + static_cast<size_t>(y) * scratchWidth;
        const double sy = static_cast<double>(y) + 0.5;
        const double dy = sy - cy;
        for (int x = x0; x <= x1; ++x) {
            const double sx = static_cast<double>(x) + 0.5;
            const double dx = sx - cx;
            if (dx * dx + dy * dy > r2) continue;
            // Keep only the half-disk that sticks out past the path end.
            if (dx * outwardX + dy * outwardY < 0.0) continue;
            row[x] = mask;
        }
    }
}

DWORD GdiJoinStyle(JoinStyle join) {
    switch (join) {
        case JoinStyle::Round:
            return PS_JOIN_ROUND;
        case JoinStyle::Bevel:
            return PS_JOIN_BEVEL;
        case JoinStyle::Miter:
        default:
            return PS_JOIN_MITER;
    }
}

}  // namespace

GdiAntialias::GdiAntialias() = default;

GdiAntialias::~GdiAntialias() {
    ReleaseBuffers();
}

void GdiAntialias::SetScale(int scale) {
    scale_ = std::clamp(scale, kMinScale, kMaxScale);
}

void GdiAntialias::ReleaseBuffers() {
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

bool GdiAntialias::EnsureScratch(int width, int height) {
    if (!scratchDC_) {
        scratchDC_ = CreateCompatibleDC(nullptr);
        if (!scratchDC_) return false;
    }
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

bool GdiAntialias::EnsureReadBack(int width, int height) {
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

void GdiAntialias::TrimBuffers() {
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

bool GdiAntialias::FillPolygon(HDC dc, const AaPoint* points, int count,
                                const Color& color) {
    BrushStyle brush;
    brush.color = color;
    return RenderShape(dc, points, count, true, nullptr, &brush);
}

bool GdiAntialias::StrokePolyline(HDC dc, const AaPoint* points, int count,
                                   bool closed, const Color& color,
                                   const PenStyle& pen) {
    PenStyle effective = pen;
    effective.color = color;
    return RenderShape(dc, points, count, closed, &effective, nullptr);
}

bool GdiAntialias::RenderShape(HDC dc, const AaPoint* points, int count,
                                bool closed, const PenStyle* pen,
                                const BrushStyle* brush) {
    if (!dc || !points || count < 2) return false;

    // Bounding box of the shape in device coordinates, plus a margin for
    // the stroke width.
    double minX = points[0].x, maxX = points[0].x;
    double minY = points[0].y, maxY = points[0].y;
    for (int i = 1; i < count; ++i) {
        minX = std::min(minX, points[i].x);
        maxX = std::max(maxX, points[i].x);
        minY = std::min(minY, points[i].y);
        maxY = std::max(maxY, points[i].y);
    }
    const double penWidth = pen ? std::max(1.0, pen->width) : 0.0;
    const double margin = penWidth + 1.0;

    const int left = static_cast<int>(std::floor(minX - margin));
    const int top = static_cast<int>(std::floor(minY - margin));
    const int width = static_cast<int>(std::ceil(maxX + margin)) - left;
    const int height = static_cast<int>(std::ceil(maxY + margin)) - top;
    if (width <= 0 || height <= 0) return false;
    if (width > kMaxDimension || height > kMaxDimension) return false;

    const int hiWidth = width * scale_;
    const int hiHeight = height * scale_;
    if (static_cast<long long>(hiWidth) * hiHeight > kMaxSamples) return false;
    if (!EnsureScratch(hiWidth, hiHeight)) return false;

    // Clear the part of the buffer that is going to be read back: uncovered
    // samples must differ from the mask color.  Only the rows of the shape
    // itself are cleared (not the whole buffer) - the buffer can be huge and
    // clearing all of it for every shape would dominate the rendering time.
    for (int y = 0; y < hiHeight; ++y) {
        DWORD* row = static_cast<DWORD*>(scratchBits_) +
                     static_cast<size_t>(y) * scratchWidth_;
        std::memset(row, 0, static_cast<size_t>(hiWidth) * sizeof(DWORD));
    }

    // Note: the raster operation of the scratch DC must not be changed.
    // SetROP2() makes GDI use its old-style raster operations, which do not
    // work with 32 bit DIB sections: the shape would be drawn in the
    // background color of the DC instead of in the color of the brush.
    // The default raster operation is SRCCOPY, which is what is needed.

    // The shape, scaled up and moved so that its bounding box starts at the
    // origin of the buffer.
    std::vector<POINT> scaled(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        scaled[static_cast<size_t>(i)].x =
            static_cast<LONG>((points[i].x - left) * scale_);
        scaled[static_cast<size_t>(i)].y =
            static_cast<LONG>((points[i].y - top) * scale_);
    }

    HPEN penHandle = nullptr;
    HBRUSH brushHandle = nullptr;
    HGDIOBJ oldPen = nullptr;
    HGDIOBJ oldBrush = nullptr;

    if (pen) {
        const int widthHi = std::max(1, static_cast<int>(std::lround(pen->width))) * scale_;
        // A round cap of an open, solid stroke is drawn analytically after
        // the body (see DrawRoundCapEnd): the body then uses flat caps so
        // the stroke stays crisp instead of carrying GDI's aliased caps.
        const bool analyticCaps = !closed && pen->cap == CapStyle::Round &&
                                  pen->dash.IsSolid();
        const DWORD capStyle = analyticCaps ? PS_ENDCAP_FLAT : GdiCapStyle(pen->cap);
        DWORD style = PS_GEOMETRIC | capStyle | GdiJoinStyle(pen->join);

        // A custom dash pattern, scaled along with the shape.
        std::vector<DWORD> dashes;
        if (!pen->dash.IsSolid()) {
            for (float dash : pen->dash.dashes) {
                float scaledDash = dash * scale_;
                if (scaledDash < 1.0f) scaledDash = 1.0f;
                dashes.push_back(static_cast<DWORD>(std::lround(scaledDash)));
            }
            // GDI wants an even number of dash elements.
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
        logBrush.lbColor = kMaskColor;
        logBrush.lbHatch = 0;
        penHandle = ExtCreatePen(style, static_cast<DWORD>(widthHi), &logBrush,
                                 static_cast<DWORD>(dashes.size()),
                                 dashes.empty() ? nullptr : dashes.data());
    } else {
        penHandle = static_cast<HPEN>(GetStockObject(NULL_PEN));
    }
    if (brush) {
        brushHandle = CreateSolidBrush(kMaskColor);
    }

    oldPen = SelectObject(scratchDC_, penHandle);
    if (brush) {
        oldBrush = SelectObject(scratchDC_, brushHandle);
    }

    if (brush) {
        SetPolyFillMode(scratchDC_, WINDING);
        Polygon(scratchDC_, scaled.data(), count);
    } else {
        SetBkMode(scratchDC_, TRANSPARENT);
        BeginPath(scratchDC_);
        Polyline(scratchDC_, scaled.data(), count);
        if (closed) {
            CloseFigure(scratchDC_);
        }
        EndPath(scratchDC_);
        StrokePath(scratchDC_);

        // Two half-disks at the open ends make up the round cap.  Writing
        // them here (after the body is drawn) keeps them anti-aliased and
        // symmetric; GDI's own PS_ENDCAP_ROUND is neither.
        const bool analyticCaps = !closed && pen && pen->cap == CapStyle::Round &&
                                  pen->dash.IsSolid();
        if (analyticCaps) {
            const double radiusHi =
                std::max(1, static_cast<int>(std::lround(pen->width))) * scale_ * 0.5;
            for (int end = 0; end < 2; ++end) {
                const AaPoint& from = end == 0 ? points[0] : points[count - 1];
                const AaPoint& to = end == 0 ? points[1] : points[count - 2];
                double outX = from.x - to.x;
                double outY = from.y - to.y;
                const double len = std::hypot(outX, outY);
                if (len > 0.0) { outX /= len; outY /= len; }
                DrawRoundCapEnd(static_cast<DWORD*>(scratchBits_), scratchWidth_, scale_,
                                from.x, from.y, outX, outY,
                                radiusHi, left, top, hiWidth, hiHeight);
            }
        }
    }

    SelectObject(scratchDC_, oldPen);
    if (brush) {
        SelectObject(scratchDC_, oldBrush);
        DeleteObject(brushHandle);
    }
    if (pen) {
        DeleteObject(penHandle);
    }

    // ---------------------------------------------------------------------
    // Filter the buffer down to device resolution.  The result is written
    // into a 32 bit DIB with *premultiplied* alpha, which is what GDI's
    // AlphaBlend expects.
    // ---------------------------------------------------------------------

    if (!EnsureReadBack(width, height)) return false;

    const DWORD* scratch = static_cast<const DWORD*>(scratchBits_);
    DWORD* readBack = static_cast<DWORD*>(readBackBits_);
    const DWORD maskDword = DwordFromColor(kMaskColor);
    const Color& shape = pen ? pen->color : brush->color;
    const int red = shape.r;
    const int green = shape.g;
    const int blue = shape.b;

    for (int y = 0; y < height; ++y) {
        const DWORD* scratchRow =
            scratch + static_cast<size_t>(y) * scale_ * scratchWidth_;
        DWORD* readBackRow = readBack + static_cast<size_t>(y) * readBackWidth_;

        for (int x = 0; x < width; ++x) {
            int covered = 0;
            for (int j = 0; j < scale_; ++j) {
                const DWORD* row =
                    scratchRow + static_cast<size_t>(j) * scratchWidth_;
                for (int i = 0; i < scale_; ++i) {
                    if ((row[x * scale_ + i] & 0x00FFFFFF) == maskDword) {
                        ++covered;
                    }
                }
            }
            if (covered == 0) {
                // Fully transparent: the destination stays untouched.
                readBackRow[x] = 0;
                continue;
            }
            if (covered == scale_ * scale_) {
                // Fully covered: the shape color, fully opaque.
                // A 32 bit DIB stores the pixels as B, G, R, A, so the DWORD
                // has to be built as 0xAARRGGBB.
                readBackRow[x] = 0xFF000000u |
                                 (static_cast<DWORD>(red) << 16) |
                                 (static_cast<DWORD>(green) << 8) |
                                 static_cast<DWORD>(blue);
                continue;
            }
            // Premultiplied alpha: the color is scaled by the coverage.
            const int alpha = (covered * 255) / (scale_ * scale_);
            readBackRow[x] = (static_cast<DWORD>(alpha) << 24) |
                              (static_cast<DWORD>(red * alpha / 255) << 16) |
                              (static_cast<DWORD>(green * alpha / 255) << 8) |
                              static_cast<DWORD>(blue * alpha / 255);
        }
    }

    // Composite the shape onto the destination.  AlphaBlend does the
    // source-over blending, so the destination does not have to be read back
    // (which would be wrong when the destination is clipped).
    // The source bitmap is a 32 bit DIB with *premultiplied* alpha, so
    // AlphaFormat has to be AC_SRC_ALPHA - any other value makes
    // AlphaBlend fail with ERROR_INVALID_PARAMETER.
    const BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    const BOOL ab = AlphaBlend(dc, left, top, width, height, readBackDC_, 0, 0,
                               width, height, blend);
    const bool ok = ab != 0;

    // The buffers are only grown, never shrunk.  A single big shape would
    // therefore keep its (huge) scratch buffer alive for the rest of the
    // widget's life - and every canvas widget has its own renderer.  Give
    // the memory back once the shape is done.
    TrimBuffers();
    return ok;
}

}  // namespace tkd2dcanvas
