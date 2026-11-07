#include "D2DRenderer.h"

#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "D2DContext.h"
#include "FontCache.h"
#include "PenCache.h"
#include "Utf16.h"

// Concrete Direct2D state. Kept out of the header so <d2d1.h> does not leak.
struct D2DRendererImpl {
    Microsoft::WRL::ComPtr<ID2D1DCRenderTarget> rt;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    HDC dc = nullptr;
    int width = 0;
    int height = 0;
    bool frameOpen = false;
    int clipDepth = 0;  // number of active axis-aligned clips (nested = intersected)
};

D2DRenderer::D2DRenderer() : impl_(std::make_unique<D2DRendererImpl>()) {}
D2DRenderer::~D2DRenderer() { Detach(); }

// ---------------------------------------------------------------------------
// Binding / frame lifecycle
// ---------------------------------------------------------------------------

bool D2DRenderer::AttachToHdc(void* hdc, int width, int height) {
    if (!hdc || width <= 0 || height <= 0) return false;
    if (!D2DContext::EnsureInitialized()) return false;

    const HDC dc = static_cast<HDC>(hdc);

    // Recreate the render target if the DC or size changed.
    if (impl_->rt && (impl_->dc != dc || impl_->width != width || impl_->height != height)) {
        Detach();
    }

    if (!impl_->rt) {
        FLOAT dpiX = 96.0f, dpiY = 96.0f;
        D2DContext::Factory()->GetDesktopDpi(&dpiX, &dpiY);

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

    const RECT rc{0, 0, width, height};
    HRESULT hr = impl_->rt->BindDC(dc, &rc);
    if (FAILED(hr)) return false;

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
            impl_->rt.Reset();
        }
    }

    impl_->dc = nullptr;
    while (impl_->clipDepth > 0) {
        impl_->rt->PopAxisAlignedClip();
        --impl_->clipDepth;
    }
}

void D2DRenderer::EnsureBrush() {
    if (impl_->brush || !impl_->rt) return;
    impl_->rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f), &impl_->brush);
}

namespace {
D2D1::ColorF ToD2DColor(const Color& c) {
    return D2D1::ColorF(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f);
}
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
    BuildGeometry(path, geometry.Get());

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
        : size * 1.25f;
    const float blockHeight = lineHeight * static_cast<float>(layout.lines.size());

    const std::string anchor = layout.anchor.empty() ? "nw" : layout.anchor;

    const auto horizontal = [&]() {
        if (anchor == "nw" || anchor == "w" || anchor == "sw") return 0;  // left
        if (anchor == "n" || anchor == "c" || anchor == "s") return 1;    // center
        return 2;                                                          // right
    };
    const auto vertical = [&]() {
        if (anchor == "nw" || anchor == "n" || anchor == "ne") return 0;  // top
        if (anchor == "w" || anchor == "c" || anchor == "e") return 1;    // middle
        return 2;                                                          // bottom
    };

    const int ha = horizontal();
    const int va = vertical();

    float blockTop = static_cast<FLOAT>(layout.origin.y);
    if (va == 1) blockTop -= blockHeight * 0.5f;
    else if (va == 2) blockTop -= blockHeight;

    impl_->brush->SetColor(ToD2DColor(style.color));

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

        DWRITE_TEXT_METRICS metrics{};
        textLayout->GetMetrics(&metrics);

        float x = static_cast<FLOAT>(layout.origin.x);
        if (ha == 1) x -= metrics.width * 0.5f;
        else if (ha == 2) x -= metrics.width;

        const float y = blockTop + lineHeight * static_cast<FLOAT>(lineIndex);
        impl_->rt->DrawTextLayout(D2D1::Point2F(x, y), textLayout.Get(), impl_->brush.Get(),
                                  D2D1_DRAW_TEXT_OPTIONS_NONE);
        ++lineIndex;
    }
}

// ---------------------------------------------------------------------------
// Images (M9 — Tk photo images converted to D2D bitmaps)
// ---------------------------------------------------------------------------

void D2DRenderer::drawImage(const ImageRef& image, const Transform2D& transform) {
    if (!impl_->rt || !impl_->frameOpen) return;

    auto* bitmap = static_cast<ID2D1Bitmap*>(const_cast<void*>(image.handle));
    if (!bitmap) return;  // M9: CanvasTkImage will supply an ID2D1Bitmap* here.

    double a, b, c, d, e, f;
    transform.Coefficients(a, b, c, d, e, f);

    D2D1_MATRIX_3X2_F previous{};
    impl_->rt->GetTransform(&previous);
    impl_->rt->SetTransform(D2D1::Matrix3x2F(
        static_cast<FLOAT>(a), static_cast<FLOAT>(b),
        static_cast<FLOAT>(c), static_cast<FLOAT>(d),
        static_cast<FLOAT>(e), static_cast<FLOAT>(f)));
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
