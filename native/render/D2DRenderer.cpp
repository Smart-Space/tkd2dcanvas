#include "D2DRenderer.h"

#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "D2DContext.h"
#include "D2DTargetPool.h"
#include "FontCache.h"
#include "PenCache.h"
#include "Utf16.h"
#include "tkd2dcanvas/Anchor.h"
#include "tkd2dcanvas/StrokeAlign.h"

namespace {

// The render target and the brush are shared by the whole process (see
// D2DTargetPool.h); the pool's lock is held while a frame is open.
ID2D1RenderTarget* SharedTarget() { return D2DTargetPool::Instance().Target(); }
ID2D1SolidColorBrush* SharedBrush() { return D2DTargetPool::Instance().Brush(); }

}  // namespace

struct CachedBitmap {
    Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
    double width = 0.0;
    double height = 0.0;
};

// Concrete Direct2D state. Kept out of the header so <d2d1.h> does not leak.
struct D2DRendererImpl {
    // Every renderer gets a number of its own: the shared render target
    // remembers which widget it is bound to, so that the binding of a
    // widget that is gone is dropped.
    unsigned ownerId = 0;
    HDC dc = nullptr;
    int width = 0;
    int height = 0;
    bool frameOpen = false;
    // The render target itself lives in the pool, which the whole process
    // shares (D2DTargetPool.h).  The guard holds the pool while the frame
    // is open: the target may only be used by one widget at a time.
    std::unique_ptr<D2DTargetPool::Guard> lock;
    int clipDepth = 0;  // number of active axis-aligned clips (nested = intersected)
    std::vector<D2D1_MATRIX_3X2_F> transformStack;  // world transforms saved by pushTranslate
    // Generation of the shared render target the bitmaps below belong to.
    // It changes when the target is recreated (device lost), and then the
    // bitmaps have to go.
    unsigned cacheGeneration = 0;
    // Bitmaps created from raw RGBA pixels (Tk photo / XBM items), keyed by
    // (cacheKey << 32) | generation. Bounded; entries are simply dropped
    // when the map grows past the limit and rebuilt on demand.
    std::unordered_map<std::uint64_t, CachedBitmap> bitmapCache;
};

D2DRenderer::D2DRenderer() : impl_(std::make_unique<D2DRendererImpl>()) {
    // The owner id has to be unique for every renderer of the process: it
    // tells the shared render target which widget it is bound to.
    static std::atomic<unsigned> nextOwnerId{0};
    impl_->ownerId = ++nextOwnerId;
}

D2DRenderer::~D2DRenderer() {
    Detach();
    // The back buffer of the widget may be destroyed right after this, so
    // the shared target must not keep a reference to it.
    D2DTargetPool::Instance().Unbind(impl_->ownerId);
}

// ---------------------------------------------------------------------------
// Binding / frame lifecycle
// ---------------------------------------------------------------------------

bool D2DRenderer::AttachToHdc(void* hdc, int width, int height) {
    if (!hdc || width <= 0 || height <= 0) return false;

    D2DTargetPool& pool = D2DTargetPool::Instance();

    // The pool is locked for the whole frame: the render target is shared
    // by all canvas widgets of the process.
    impl_->lock.reset();
    impl_->lock = std::make_unique<D2DTargetPool::Guard>();

    if (!pool.EnsureTarget()) {
        impl_->lock.reset();
        return false;
    }

    // The bitmaps of the cache belong to the render target: when the target
    // was recreated, they are not valid any more.
    if (impl_->cacheGeneration != pool.Generation()) {
        impl_->bitmapCache.clear();
        impl_->cacheGeneration = pool.Generation();
    }

    const HDC dc = static_cast<HDC>(hdc);
    if (!pool.Bind(dc, width, height, impl_->ownerId)) {
        impl_->lock.reset();
        return false;
    }

    impl_->dc = dc;
    impl_->width = width;
    impl_->height = height;

    EnsureBrush();

    pool.Target()->BeginDraw();
    impl_->frameOpen = true;
    return true;
}

void D2DRenderer::Detach() {
    D2DTargetPool& pool = D2DTargetPool::Instance();

    if (pool.Target() && impl_->frameOpen) {
        // The clip stack and the transform belong to the render target, and
        // the render target is shared by all canvas widgets: they have to
        // be restored before the frame ends, otherwise they would still be
        // in effect for the next widget that paints.
        while (impl_->clipDepth > 0) {
            pool.Target()->PopAxisAlignedClip();
            --impl_->clipDepth;
        }
        pool.Target()->SetTransform(D2D1::Matrix3x2F::Identity());
        impl_->transformStack.clear();

        const HRESULT hr = pool.Target()->EndDraw();
        impl_->frameOpen = false;
        if (hr == D2DERR_RECREATE_TARGET) {
            // Device lost: drop the render target; the next AttachToHdc
            // recreates it.
            pool.HandleDeviceLost();
            impl_->bitmapCache.clear();
        }
    } else {
        impl_->clipDepth = 0;
        impl_->transformStack.clear();
    }

    impl_->dc = nullptr;

    // The binding is kept: the next paint of this widget uses the same DC
    // and does not have to bind the target again.  The renderer releases it
    // when it goes away (see the destructor) or when the back buffer of
    // the widget was replaced (see ResetBinding).
    impl_->lock.reset();
}

void D2DRenderer::ResetBinding() {
    D2DTargetPool& pool = D2DTargetPool::Instance();

    if (pool.Target() && impl_->frameOpen) {
        while (impl_->clipDepth > 0) {
            pool.Target()->PopAxisAlignedClip();
            --impl_->clipDepth;
        }
        pool.Target()->SetTransform(D2D1::Matrix3x2F::Identity());
        impl_->transformStack.clear();

        if (pool.Target()->EndDraw() == D2DERR_RECREATE_TARGET) {
            pool.HandleDeviceLost();
            impl_->bitmapCache.clear();
        }
        impl_->frameOpen = false;
    }
    impl_->clipDepth = 0;
    impl_->transformStack.clear();
    // The back buffer was replaced: the shared target has to be bound to
    // the new one before the next frame.
    pool.Unbind(impl_->ownerId);
}

void D2DRenderer::EnsureBrush() {
    D2DTargetPool::Instance().Brush();
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
    if (!SharedTarget() || !impl_->frameOpen) return;
    SharedTarget()->Clear(ToD2DColor(color));
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
    if (!SharedTarget() || !impl_->frameOpen || path.Size() < 2 || !SharedBrush()) return;

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

    SharedBrush()->SetColor(ToD2DColor(pen.color));
    SharedTarget()->DrawGeometry(geometry.Get(), SharedBrush(),
                            static_cast<FLOAT>(pen.width), stroke.Get());
}

void D2DRenderer::fillPath(const Path& path, const BrushStyle& brush) {
    if (!SharedTarget() || !impl_->frameOpen || path.Size() < 3 || !SharedBrush()) return;

    Microsoft::WRL::ComPtr<ID2D1PathGeometry> geometry;
    if (FAILED(D2DContext::Factory()->CreatePathGeometry(&geometry))) return;
    BuildGeometry(path, geometry.Get());

    SharedBrush()->SetColor(ToD2DColor(brush.color));
    SharedTarget()->FillGeometry(geometry.Get(), SharedBrush());
}

void D2DRenderer::drawPath(const Path& path, const PenStyle& pen, const BrushStyle* brush) {
    if (brush) fillPath(path, *brush);
    drawLine(path, pen);
}

// ---------------------------------------------------------------------------
// Text (DirectWrite)
// ---------------------------------------------------------------------------

void D2DRenderer::drawText(const TextLayout& layout, const TextStyle& style) {
    if (!SharedTarget() || !impl_->frameOpen || !SharedBrush() || layout.lines.empty()) return;
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

    SharedBrush()->SetColor(ToD2DColor(style.color));

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

        SharedTarget()->GetTransform(&savedTransform);
        const D2D1::Matrix3x2F rot(cosA, -sinA, sinA, cosA, 0.0f, 0.0f);
        const D2D1::Matrix3x2F worldWithOrigin = Compose3x2(
            D2D1::Matrix3x2F::Translation(static_cast<FLOAT>(ox),
                                          static_cast<FLOAT>(oy)),
            savedTransform);
        const D2D1::Matrix3x2F composed = Compose3x2(rot, worldWithOrigin);
        SharedTarget()->SetTransform(composed);

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
        SharedTarget()->DrawTextLayout(D2D1::Point2F(static_cast<FLOAT>(x), y), textLayout.Get(),
                                  SharedBrush(), D2D1_DRAW_TEXT_OPTIONS_NONE);
        ++lineIndex;
    }

    if (angle != 0.0) {
        SharedTarget()->SetTransform(savedTransform);
    }
}

// ---------------------------------------------------------------------------
// Images (Tk photo / XBM bitmap images converted to D2D bitmaps)
// ---------------------------------------------------------------------------

void D2DRenderer::drawImage(const ImageRef& image, const Transform2D& transform) {
    if (!SharedTarget() || !impl_->frameOpen) return;

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
            HRESULT hr = SharedTarget()->CreateBitmap(
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
    SharedTarget()->GetTransform(&previous);
    const D2D1::Matrix3x2F composed = Compose3x2(
        D2D1::Matrix3x2F(static_cast<FLOAT>(a), static_cast<FLOAT>(b),
                         static_cast<FLOAT>(c), static_cast<FLOAT>(d),
                         static_cast<FLOAT>(e), static_cast<FLOAT>(f)),
        previous);
    SharedTarget()->SetTransform(composed);
    SharedTarget()->DrawBitmap(bitmap,
                          D2D1::RectF(0.0f, 0.0f,
                                      static_cast<FLOAT>(image.width),
                                      static_cast<FLOAT>(image.height)));
    SharedTarget()->SetTransform(previous);
}

// ---------------------------------------------------------------------------
// Clipping
// ---------------------------------------------------------------------------

void D2DRenderer::pushClip(const BBox& box) {
    if (!SharedTarget() || !impl_->frameOpen || !box.IsValid()) return;

    const D2D1_RECT_F rect{
        static_cast<FLOAT>(box.left), static_cast<FLOAT>(box.top),
        static_cast<FLOAT>(box.right), static_cast<FLOAT>(box.bottom)};
    // Nested axis-aligned clips intersect in D2D.
    SharedTarget()->PushAxisAlignedClip(rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    ++impl_->clipDepth;
}

void D2DRenderer::popClip() {
    if (!SharedTarget() || !impl_->frameOpen) return;
    if (impl_->clipDepth <= 0) return;

    SharedTarget()->PopAxisAlignedClip();
    --impl_->clipDepth;
}

void D2DRenderer::pushTranslate(double dx, double dy) {
    if (!SharedTarget() || !impl_->frameOpen) return;

    D2D1_MATRIX_3X2_F previous{};
    SharedTarget()->GetTransform(&previous);
    impl_->transformStack.push_back(previous);
    // The new translation applies in *inner* coordinates, so it multiplies
    // on the left of the current transform: v' = (v*T)*previous.
    const D2D1::Matrix3x2F composed = Compose3x2(
        D2D1::Matrix3x2F::Translation(static_cast<FLOAT>(dx),
                                      static_cast<FLOAT>(dy)),
        previous);
    SharedTarget()->SetTransform(composed);
}

void D2DRenderer::popTranslate() {
    if (!SharedTarget() || !impl_->frameOpen) return;
    if (impl_->transformStack.empty()) return;

    D2D1_MATRIX_3X2_F previous = impl_->transformStack.back();
    impl_->transformStack.pop_back();
    SharedTarget()->SetTransform(previous);
}
