#include "D2DRenderer.h"

#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "D2DContext.h"
#include "FontCache.h"
#include "PenCache.h"
#include "Utf16.h"
#include "tkd2dcanvas/Anchor.h"
#include "tkd2dcanvas/StrokeAlign.h"

struct CachedBitmap {
    Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
    double width = 0.0;
    double height = 0.0;
};

// Concrete Direct2D state. Kept out of the header so <d2d1.h> does not leak.
struct D2DRendererImpl {
    Microsoft::WRL::ComPtr<ID2D1DCRenderTarget> rt;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    HDC dc = nullptr;
    int width = 0;
    int height = 0;
    bool frameOpen = false;
    HDC boundDc = nullptr;  // HDC/rect the render target is currently bound to
    int boundWidth = 0;
    int boundHeight = 0;
    int clipDepth = 0;  // number of active axis-aligned clips (nested = intersected)
    std::vector<D2D1_MATRIX_3X2_F> transformStack;  // world transforms saved by pushTranslate
    // Bitmaps created from raw RGBA pixels (Tk photo / XBM items), keyed by
    // (cacheKey << 32) | generation. Bounded; entries are simply dropped
    // when the map grows past the limit and rebuilt on demand.
    std::unordered_map<std::uint64_t, CachedBitmap> bitmapCache;
};

D2DRenderer::D2DRenderer() : impl_(std::make_unique<D2DRendererImpl>()) {}
D2DRenderer::~D2DRenderer() { Detach(); }

// ---------------------------------------------------------------------------
// Binding / frame lifecycle
// ---------------------------------------------------------------------------

bool D2DRenderer::AttachToHdc(void* hdc, int width, int height) {
    if (!hdc || width <= 0 || height <= 0) return false;
    if (!D2DContext::EnsureInitialized()) {
        return false;
    }

    const HDC dc = static_cast<HDC>(hdc);

    // Recreate the render target if the DC or size changed.
    if (impl_->rt && (impl_->dc != dc || impl_->width != width || impl_->height != height)) {
        Detach();
    }

    if (!impl_->rt) {
        // Render in pixel space: 1 D2D unit = 1 back-buffer pixel, matching
        // Tk's logical-pixel coordinates (and the RECT passed to BindDC).
        // Using GetDesktopDpi() here would scale everything by the display's
        // scale factor (e.g. 2x) and clip content to the top-left quarter of
        // the buffer. Per-monitor DPI handling is deferred to Win32Dpi.
        const FLOAT dpiX = 96.0f, dpiY = 96.0f;

        const D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,   // hardware, transparent WARP fallback
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
            dpiX, dpiY);

        HRESULT hr = D2DContext::Factory()->CreateDCRenderTarget(&props, &impl_->rt);
        if (FAILED(hr)) {
            // Explicit software fallback for machines without usable D3D.
            const D2D1_RENDER_TARGET_PROPERTIES swProps = D2D1::RenderTargetProperties(
                D2D1_RENDER_TARGET_TYPE_SOFTWARE,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
                dpiX, dpiY);
            hr = D2DContext::Factory()->CreateDCRenderTarget(&swProps, &impl_->rt);
            if (FAILED(hr)) {
                impl_->rt.Reset();
                return false;
            }
        }
        impl_->rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        impl_->rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    }

    impl_->dc = dc;
    impl_->width = width;
    impl_->height = height;

    if (impl_->boundDc != dc || impl_->boundWidth != width ||
        impl_->boundHeight != height) {
        const RECT rc{0, 0, width, height};
        HRESULT hr = impl_->rt->BindDC(dc, &rc);
        if (FAILED(hr)) {
            return false;
        }
        impl_->boundDc = dc;
        impl_->boundWidth = width;
        impl_->boundHeight = height;
    }

    EnsureBrush();

    impl_->rt->BeginDraw();
    impl_->frameOpen = true;
    return true;
}

void D2DRenderer::Detach() {
    if (!impl_->rt) return;

    if (impl_->frameOpen) {
        const HRESULT hr = impl_->rt->EndDraw();
        impl_->frameOpen = false;
        if (hr == D2DERR_RECREATE_TARGET) {
            // Device lost: drop the render target; next AttachToHdc recreates it.
            impl_->brush.Reset();
            impl_->bitmapCache.clear();
            impl_->rt.Reset();
            impl_->boundDc = nullptr;
            impl_->boundWidth = 0;
            impl_->boundHeight = 0;
        }
    }

    impl_->dc = nullptr;
    while (impl_->clipDepth > 0) {
        impl_->rt->PopAxisAlignedClip();
        --impl_->clipDepth;
    }
    impl_->transformStack.clear();
}

void D2DRenderer::ResetBinding() {
    if (impl_->frameOpen && impl_->rt) {
        if (impl_->rt->EndDraw() == D2DERR_RECREATE_TARGET) {
            impl_->brush.Reset();
            impl_->bitmapCache.clear();
            impl_->rt.Reset();
        }
        impl_->frameOpen = false;
    }
    impl_->boundDc = nullptr;
    impl_->boundWidth = 0;
    impl_->boundHeight = 0;
}

void D2DRenderer::EnsureBrush() {
    if (impl_->brush || !impl_->rt) return;
    impl_->rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f), &impl_->brush);
}

namespace {
D2D1::ColorF ToD2DColor(const Color& c) {
    return D2D1::ColorF(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f);
}

// Row-vector affine composition C = A * B (v' = (v*A)*B), where the
// matrices use D2D1::Matrix3x2F's layout (_11 _12 _21 _22 _31 _32).
D2D1::Matrix3x2F Compose3x2(const D2D1_MATRIX_3X2_F& A,
                             const D2D1_MATRIX_3X2_F& B) {
    return D2D1::Matrix3x2F(
        A._11 * B._11 + A._12 * B._21,
        A._11 * B._12 + A._12 * B._22,
        A._21 * B._11 + A._22 * B._21,
        A._21 * B._12 + A._22 * B._22,
        A._31 * B._11 + A._32 * B._21 + B._31,
        A._31 * B._12 + A._32 * B._22 + B._32);
}

// Pixel alignment for strokes.
//
// See tkd2dcanvas/StrokeAlign.h: the stroke edges are moved onto the pixel
// grid so that a 1px border stays 1px wide and crisp, and so that items
// which are meant to line up share the same sub-pixel phase.
}  // namespace

void D2DRenderer::clear(const Color& color) {
    if (!impl_->rt || !impl_->frameOpen) return;
    impl_->rt->Clear(ToD2DColor(color));
}

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

void D2DRenderer::BuildGeometry(const Path& path, ID2D1PathGeometry* geometry) const {
    if (!geometry || path.points.empty()) return;

    Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(geometry->Open(&sink))) return;

    sink->SetFillMode(D2D1_FILL_MODE_ALTERNATE);
    sink->BeginFigure(
        D2D1::Point2F(static_cast<FLOAT>(path.points[0].x), static_cast<FLOAT>(path.points[0].y)),
        path.closed ? D2D1_FIGURE_BEGIN_FILLED : D2D1_FIGURE_BEGIN_HOLLOW);

    for (std::size_t i = 1; i < path.points.size(); ++i) {
        sink->AddLine(D2D1::Point2F(static_cast<FLOAT>(path.points[i].x),
                                    static_cast<FLOAT>(path.points[i].y)));
    }
    sink->EndFigure(path.closed ? D2D1_FIGURE_END_CLOSED : D2D1_FIGURE_END_OPEN);
    sink->Close();
}

// ---------------------------------------------------------------------------
// Drawing primitives
// ---------------------------------------------------------------------------

void D2DRenderer::drawLine(const Path& path, const PenStyle& pen) {
    if (!impl_->rt || !impl_->frameOpen || path.Size() < 2 || !impl_->brush) return;

    Microsoft::WRL::ComPtr<ID2D1PathGeometry> geometry;
    if (FAILED(D2DContext::Factory()->CreatePathGeometry(&geometry))) return;

    // Half-pixel stroke edges are moved onto the pixel grid (see
    // AlignStrokePath), so that a 1px border stays 1px wide and crisp.
    // The buffers are members: aligning a stroke must not allocate.
    if (tkd2dcanvas::AlignStrokePath(path, pen.width, pen.cap, pen.capEnds,
                                      alignedPath_, alignEdgesX_, alignEdgesY_)) {
        BuildGeometry(alignedPath_, geometry.Get());
    } else {
        BuildGeometry(path, geometry.Get());
    }

    auto stroke = PenCache::Instance().GetStrokeStyle(D2DContext::Factory(), pen);

    impl_->brush->SetColor(ToD2DColor(pen.color));
    impl_->rt->DrawGeometry(geometry.Get(), impl_->brush.Get(),
                            static_cast<FLOAT>(pen.width), stroke.Get());
}

void D2DRenderer::fillPath(const Path& path, const BrushStyle& brush) {
    if (!impl_->rt || !impl_->frameOpen || path.Size() < 3 || !impl_->brush) return;

    Microsoft::WRL::ComPtr<ID2D1PathGeometry> geometry;
    if (FAILED(D2DContext::Factory()->CreatePathGeometry(&geometry))) return;
    BuildGeometry(path, geometry.Get());

    impl_->brush->SetColor(ToD2DColor(brush.color));
    impl_->rt->FillGeometry(geometry.Get(), impl_->brush.Get());
}

void D2DRenderer::drawPath(const Path& path, const PenStyle& pen, const BrushStyle* brush) {
    if (brush) fillPath(path, *brush);
    drawLine(path, pen);
}

// ---------------------------------------------------------------------------
// Text (DirectWrite)
// ---------------------------------------------------------------------------

void D2DRenderer::drawText(const TextLayout& layout, const TextStyle& style) {
    if (!impl_->rt || !impl_->frameOpen || !impl_->brush || layout.lines.empty()) return;
    IDWriteFactory* writeFactory = D2DContext::WriteFactory();
    if (!writeFactory) return;

    const float size = style.size > 0.0 ? static_cast<float>(style.size) : 12.0f;
    TextStyle effective = style;
    effective.size = size;

    auto format = FontCache::Instance().GetFormat(writeFactory, effective);
    if (!format) return;

    const float lineHeight = layout.lineSpacing > 0.0
        ? static_cast<float>(layout.lineSpacing)
        : static_cast<float>(TextLineHeight(size));
    const float blockHeight = lineHeight * static_cast<float>(layout.lines.size());

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
                Microsoft::WRL::ComPtr<IDWriteTextLayout> metricsLayout;
                HRESULT hr = writeFactory->CreateTextLayout(
                    wideLine.c_str(), static_cast<UINT32>(wideLine.size()),
                    format.Get(), 1.0e6f, 1.0e4f, &metricsLayout);
                if (SUCCEEDED(hr) && metricsLayout) {
                    DWRITE_TEXT_METRICS metrics{};
                    metricsLayout->GetMetrics(&metrics);
                    lineWidth = static_cast<double>(metrics.width);
                }
            }
            lineWidths.push_back(lineWidth);
            measuredBlockWidth = std::max(measuredBlockWidth, lineWidth);
        }
    }
    const double blockWidth = layout.blockWidth > 0.0 ? layout.blockWidth : measuredBlockWidth;

    const std::string justify = layout.justify.empty() ? "left" : layout.justify;

    impl_->brush->SetColor(ToD2DColor(style.color));

    const double angle = layout.angle;
    const std::string anchor = layout.anchor.empty() ? "nw" : layout.anchor;
    Point topLeft = AnchorTopLeft(anchor, layout.origin.x, layout.origin.y,
                                  blockWidth, static_cast<double>(blockHeight));

    D2D1_MATRIX_3X2_F savedTransform{};
    if (angle != 0.0) {
        const double radians = angle * (3.14159265358979323846 / 180.0);
        const FLOAT cosA = static_cast<FLOAT>(std::cos(radians));
        const FLOAT sinA = static_cast<FLOAT>(std::sin(radians));

        const double offsetX = AnchorOffsetX(anchor, blockWidth);
        const double offsetY = AnchorOffsetY(anchor, blockHeight);
        const double ox = layout.origin.x + offsetX * cosA + offsetY * sinA;
        const double oy = layout.origin.y + offsetY * cosA - offsetX * sinA;

        impl_->rt->GetTransform(&savedTransform);
        const D2D1::Matrix3x2F rot(cosA, -sinA, sinA, cosA, 0.0f, 0.0f);
        const D2D1::Matrix3x2F worldWithOrigin = Compose3x2(
            D2D1::Matrix3x2F::Translation(static_cast<FLOAT>(ox),
                                          static_cast<FLOAT>(oy)),
            savedTransform);
        const D2D1::Matrix3x2F composed = Compose3x2(rot, worldWithOrigin);
        impl_->rt->SetTransform(composed);

        topLeft = Point{0.0, 0.0};
    }

    int lineIndex = 0;
    for (const std::string& line : layout.lines) {
        const std::wstring wideLine = Utf8ToWide(line);
        if (wideLine.empty()) {
            ++lineIndex;
            continue;
        }

        Microsoft::WRL::ComPtr<IDWriteTextLayout> textLayout;
        HRESULT hr = writeFactory->CreateTextLayout(
            wideLine.c_str(), static_cast<UINT32>(wideLine.size()),
            format.Get(), 1.0e6f, 1.0e4f, &textLayout);
        if (FAILED(hr) || !textLayout) { ++lineIndex; continue; }

        if (style.underline || style.overstrike) {
            DWRITE_TEXT_RANGE range{0, static_cast<UINT32>(wideLine.size())};
            if (style.underline) textLayout->SetUnderline(TRUE, range);
            if (style.overstrike) textLayout->SetStrikethrough(TRUE, range);
        }

        const double x = topLeft.x + JustifyOffset(justify, blockWidth, lineWidths[lineIndex]);
        const float y = static_cast<FLOAT>(topLeft.y) + lineHeight * static_cast<FLOAT>(lineIndex);
        impl_->rt->DrawTextLayout(D2D1::Point2F(static_cast<FLOAT>(x), y), textLayout.Get(),
                                  impl_->brush.Get(), D2D1_DRAW_TEXT_OPTIONS_NONE);
        ++lineIndex;
    }

    if (angle != 0.0) {
        impl_->rt->SetTransform(savedTransform);
    }
}

// ---------------------------------------------------------------------------
// Images (Tk photo / XBM bitmap images converted to D2D bitmaps)
// ---------------------------------------------------------------------------

void D2DRenderer::drawImage(const ImageRef& image, const Transform2D& transform) {
    if (!impl_->rt || !impl_->frameOpen) return;

    ID2D1Bitmap* bitmap = nullptr;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> owned;  // holds a one-shot bitmap for this call

    if (image.pixels && image.width > 0 && image.height > 0) {
        // Raw RGBA from a Tk photo / XBM: convert once, cache by (key, generation).
        const std::uint64_t key = image.cacheKey
            ? ((static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(image.cacheKey)) << 32) |
               static_cast<std::uint64_t>(image.generation))
            : 0;
        CachedBitmap* cached = nullptr;
        if (key != 0) {
            auto it = impl_->bitmapCache.find(key);
            if (it != impl_->bitmapCache.end() &&
                it->second.width == static_cast<double>(image.width) &&
                it->second.height == static_cast<double>(image.height)) {
                cached = &it->second;
            }
        }

        if (!cached) {
            const int stride = image.stride > 0 ? image.stride : image.width * 4;
            const std::size_t rowBytes = static_cast<std::size_t>(image.width) * 4;
            std::vector<std::uint8_t> premul(rowBytes * static_cast<std::size_t>(image.height));
            if (stride == static_cast<int>(rowBytes)) {
                for (std::size_t i = 0; i < premul.size(); i += 4) {
                    const unsigned char a = image.pixels[i + 3];
                    premul[i + 0] = static_cast<std::uint8_t>((image.pixels[i + 0] * a + 127) / 255);
                    premul[i + 1] = static_cast<std::uint8_t>((image.pixels[i + 1] * a + 127) / 255);
                    premul[i + 2] = static_cast<std::uint8_t>((image.pixels[i + 2] * a + 127) / 255);
                    premul[i + 3] = a;
                }
            } else {
                for (int y = 0; y < image.height; ++y) {
                    const unsigned char* src = image.pixels + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride);
                    std::uint8_t* dst = premul.data() + static_cast<std::size_t>(y) * rowBytes;
                    for (int x = 0; x < image.width; ++x) {
                        const unsigned char a = src[x * 4 + 3];
                        dst[x * 4 + 0] = static_cast<std::uint8_t>((src[x * 4 + 0] * a + 127) / 255);
                        dst[x * 4 + 1] = static_cast<std::uint8_t>((src[x * 4 + 1] * a + 127) / 255);
                        dst[x * 4 + 2] = static_cast<std::uint8_t>((src[x * 4 + 2] * a + 127) / 255);
                        dst[x * 4 + 3] = a;
                    }
                }
            }

            const D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(
                D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.0f, 96.0f);
            CachedBitmap entry;
            HRESULT hr = impl_->rt->CreateBitmap(
                D2D1::SizeU(static_cast<UINT32>(image.width), static_cast<UINT32>(image.height)),
                premul.data(), static_cast<UINT32>(rowBytes),
                props, &entry.bitmap);
            if (FAILED(hr)) return;
            entry.width = static_cast<double>(image.width);
            entry.height = static_cast<double>(image.height);
            if (key != 0) {
                if (impl_->bitmapCache.size() >= 64) impl_->bitmapCache.clear();
                cached = &impl_->bitmapCache.emplace(key, std::move(entry)).first->second;
            } else {
                owned = std::move(entry.bitmap);
                bitmap = owned.Get();
            }
        }
        if (cached) bitmap = cached->bitmap.Get();
    } else {
        bitmap = static_cast<ID2D1Bitmap*>(const_cast<void*>(image.handle));
    }

    if (!bitmap) return;

    double a, b, c, d, e, f;
    transform.Coefficients(a, b, c, d, e, f);

    D2D1_MATRIX_3X2_F previous{};
    impl_->rt->GetTransform(&previous);
    const D2D1::Matrix3x2F composed = Compose3x2(
        D2D1::Matrix3x2F(static_cast<FLOAT>(a), static_cast<FLOAT>(b),
                         static_cast<FLOAT>(c), static_cast<FLOAT>(d),
                         static_cast<FLOAT>(e), static_cast<FLOAT>(f)),
        previous);
    impl_->rt->SetTransform(composed);
    impl_->rt->DrawBitmap(bitmap,
                          D2D1::RectF(0.0f, 0.0f,
                                      static_cast<FLOAT>(image.width),
                                      static_cast<FLOAT>(image.height)));
    impl_->rt->SetTransform(previous);
}

// ---------------------------------------------------------------------------
// Clipping
// ---------------------------------------------------------------------------

void D2DRenderer::pushClip(const BBox& box) {
    if (!impl_->rt || !impl_->frameOpen || !box.IsValid()) return;

    const D2D1_RECT_F rect{
        static_cast<FLOAT>(box.left), static_cast<FLOAT>(box.top),
        static_cast<FLOAT>(box.right), static_cast<FLOAT>(box.bottom)};
    // Nested axis-aligned clips intersect in D2D.
    impl_->rt->PushAxisAlignedClip(rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    ++impl_->clipDepth;
}

void D2DRenderer::popClip() {
    if (!impl_->rt || !impl_->frameOpen) return;
    if (impl_->clipDepth <= 0) return;

    impl_->rt->PopAxisAlignedClip();
    --impl_->clipDepth;
}

void D2DRenderer::pushTranslate(double dx, double dy) {
    if (!impl_->rt || !impl_->frameOpen) return;

    D2D1_MATRIX_3X2_F previous{};
    impl_->rt->GetTransform(&previous);
    impl_->transformStack.push_back(previous);
    // The new translation applies in *inner* coordinates, so it multiplies
    // on the left of the current transform: v' = (v*T)*previous.
    const D2D1::Matrix3x2F composed = Compose3x2(
        D2D1::Matrix3x2F::Translation(static_cast<FLOAT>(dx),
                                      static_cast<FLOAT>(dy)),
        previous);
    impl_->rt->SetTransform(composed);
}

void D2DRenderer::popTranslate() {
    if (!impl_->rt || !impl_->frameOpen) return;
    if (impl_->transformStack.empty()) return;

    D2D1_MATRIX_3X2_F previous = impl_->transformStack.back();
    impl_->transformStack.pop_back();
    impl_->rt->SetTransform(previous);
}
