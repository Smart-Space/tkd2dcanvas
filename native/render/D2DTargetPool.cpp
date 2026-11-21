// D2DTargetPool.cpp — the process-wide Direct2D render target.
#include "D2DTargetPool.h"

#include "D2DContext.h"

namespace {

long long g_targetsCreated = 0;

}  // namespace

D2DTargetPool& D2DTargetPool::Instance(D2DTargetKind kind) {
    // A function local static: it is created on first use and destroyed
    // when the module goes away (the render target is released then).
    static D2DTargetPool hardware(D2DTargetKind::Hardware);
    static D2DTargetPool software(D2DTargetKind::Software);
    return kind == D2DTargetKind::Software ? software : hardware;
}

D2DTargetPool::D2DTargetPool(D2DTargetKind kind) : kind_(kind) {
    InitializeCriticalSection(&lock_);
}

D2DTargetPool::~D2DTargetPool() {
    brush_.Reset();
    rt_.Reset();
    DeleteCriticalSection(&lock_);
}

bool D2DTargetPool::EnsureTarget() {
    if (rt_) return true;
    if (!D2DContext::EnsureInitialized()) return false;

    // Render in pixel space: 1 D2D unit = 1 back-buffer pixel, matching
    // Tk's logical-pixel coordinates (and the RECT passed to Bind).
    // Using GetDesktopDpi() here would scale everything by the display's
    // scale factor (e.g. 2x) and clip content to the top-left quarter of
    // the buffer. Per-monitor DPI handling is deferred to Win32Dpi.
    const FLOAT dpiX = 96.0f, dpiY = 96.0f;

    // The canvas draws into a memory DC (the double buffer) and blits that
    // to the window.  With the hardware back end every frame is rasterized
    // by the GPU and copied back into the DC - and the D3D device keeps the
    // memory of those copies in its own cache, which grows in big steps
    // (see docs/gdi_antialias.md 8.14).  The software back end rasterizes
    // straight into the DC: no copy, no cache.
    const D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        kind_ == D2DTargetKind::Software ? D2D1_RENDER_TARGET_TYPE_SOFTWARE
                                          : D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        dpiX, dpiY);

    HRESULT hr = D2DContext::Factory()->CreateDCRenderTarget(&props, &rt_);
    if (FAILED(hr) && kind_ != D2DTargetKind::Software) {
        // Explicit software fallback for machines without usable D3D.
        const D2D1_RENDER_TARGET_PROPERTIES swProps = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_SOFTWARE,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
            dpiX, dpiY);
        hr = D2DContext::Factory()->CreateDCRenderTarget(&swProps, &rt_);
    }
    if (FAILED(hr)) {
        rt_.Reset();
        return false;
    }

    rt_->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    rt_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);

    ++g_targetsCreated;
    ++generation_;
    boundDc_ = nullptr;
    boundWidth_ = 0;
    boundHeight_ = 0;
    boundOwner_ = 0;
    return true;
}

bool D2DTargetPool::Bind(HDC dc, int width, int height, unsigned owner) {
    if (!rt_ || !dc || width <= 0 || height <= 0) return false;
    if (boundOwner_ == owner && boundDc_ == dc && boundWidth_ == width &&
        boundHeight_ == height) {
        return true;
    }
    const RECT rc{0, 0, width, height};
    if (FAILED(rt_->BindDC(dc, &rc))) return false;
    boundDc_ = dc;
    boundWidth_ = width;
    boundHeight_ = height;
    boundOwner_ = owner;
    return true;
}

void D2DTargetPool::Unbind(unsigned owner) {
    // The render target keeps the device context it was bound to.  The
    // context belongs to a widget and can be destroyed at any time, so the
    // binding is forgotten here: the next Bind() binds a fresh one.
    //
    // The lock is taken here (the critical section is recursive, so it is
    // safe to call this while the pool is held).
    EnterCriticalSection(&lock_);
    if (owner != 0 && boundOwner_ != owner) {
        LeaveCriticalSection(&lock_);
        return;
    }
    boundDc_ = nullptr;
    boundWidth_ = 0;
    boundHeight_ = 0;
    boundOwner_ = 0;
    LeaveCriticalSection(&lock_);
}

void D2DTargetPool::HandleDeviceLost() {
    brush_.Reset();
    rt_.Reset();
    boundDc_ = nullptr;
    boundWidth_ = 0;
    boundHeight_ = 0;
    boundOwner_ = 0;
    ++generation_;
}

ID2D1SolidColorBrush* D2DTargetPool::Brush() {
    if (!brush_ && rt_) {
        rt_->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f), &brush_);
    }
    return brush_.Get();
}

long long D2DTargetPool::TargetsCreated() {
    return g_targetsCreated;
}
