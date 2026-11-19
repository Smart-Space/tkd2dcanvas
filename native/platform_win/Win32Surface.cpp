#include "Win32Surface.h"

#include "D2DRenderer.h"
#include "GdiRenderer.h"

#include "tkd2dcanvas/Geometry.h"

Win32Surface::Win32Surface() = default;
Win32Surface::~Win32Surface() = default;

void Win32Surface::AttachToWindow(HWND hwnd) {
    hwnd_ = hwnd;
    switch (rendererKind_) {
        case RendererKind::Gdi:
            renderer_ = std::make_unique<tkd2dcanvas::GdiRenderer>();
            break;
        case RendererKind::Direct2D:
        default:
            renderer_ = std::make_unique<D2DRenderer>();
            break;
    }
    SetAntiAliasScale(aaScale_);
}

void Win32Surface::Detach() {
    if (renderer_) renderer_->Detach();
    renderer_.reset();
    backBuffer_.Reset();
    hwnd_ = nullptr;
    width_ = 0;
    height_ = 0;
}

void Win32Surface::SetAntiAliasScale(int scale) {
    aaScale_ = scale;
    if (auto* gdi = dynamic_cast<tkd2dcanvas::GdiRenderer*>(renderer_.get())) {
        gdi->SetAntiAliasScale(scale);
    }
}

void Win32Surface::Resize(int width, int height) {
    width_ = width < 0 ? 0 : width;
    height_ = height < 0 ? 0 : height;
    // Buffer is recreated lazily on the next paint (size may match).
}

void Win32Surface::Invalidate(const BBox& box) {
    if (!hwnd_ || !box.IsValid()) return;
    RECT rc{};
    rc.left = static_cast<LONG>(box.left);
    rc.top = static_cast<LONG>(box.top);
    rc.right = static_cast<LONG>(box.right) + 1;  // GDI rects are exclusive
    rc.bottom = static_cast<LONG>(box.bottom) + 1;
    InvalidateRect(hwnd_, &rc, FALSE);
}

void Win32Surface::InvalidateAll() {
    if (!hwnd_) return;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

std::vector<RECT> Win32Surface::UpdateRegion(HWND hwnd) {
    std::vector<RECT> rects;
    if (!hwnd) return rects;

    HRGN region = CreateRectRgn(0, 0, 0, 0);
    if (!region) return rects;

    const int type = GetUpdateRgn(hwnd, region, FALSE);
    if (type != ERROR && type != NULLREGION) {
        const DWORD bytes = GetRegionData(region, 0, nullptr);
        if (bytes > sizeof(RGNDATAHEADER)) {
            // DWORD-aligned storage keeps RGNDATA/RECT access well aligned.
            std::vector<DWORD> buffer((bytes + sizeof(DWORD) - 1) / sizeof(DWORD));
            auto* data = reinterpret_cast<RGNDATA*>(buffer.data());
            if (GetRegionData(region, bytes, data) &&
                data->rdh.nCount != 0) {
                const RECT* first = reinterpret_cast<const RECT*>(data->Buffer);
                rects.assign(first, first + data->rdh.nCount);
            }
        }
    }

    DeleteObject(region);
    return rects;
}

bool Win32Surface::ScrollContent(int dx, int dy) {
    // The back buffer must match the current window size: a resize before the
    // next paint would otherwise scroll stale content.
    if (!hwnd_ || width_ <= 0 || height_ <= 0) return false;
    if (backBuffer_.Width() != width_ || backBuffer_.Height() != height_) return false;

    RECT client{};
    if (!GetClientRect(hwnd_, &client)) return false;
    if (client.right - client.left != width_ || client.bottom - client.top != height_) {
        return false;  // DPI/scaling mismatch: fall back to a full repaint
    }

    return backBuffer_.ScrollContent(dx, dy);
}

void Win32Surface::Paint(HDC hdc, const std::vector<RECT>& damaged,
                         const Painter& painter, int destX, int destY) {
    if (!hdc || width_ <= 0 || height_ <= 0 || damaged.empty()) return;

    // Clamp the damage to the buffer and drop empty rectangles.
    const RECT bounds{0, 0, width_, height_};
    std::vector<RECT> rects;
    rects.reserve(damaged.size());
    for (const RECT& rect : damaged) {
        RECT clipped{};
        if (IntersectRect(&clipped, &rect, &bounds)) rects.push_back(clipped);
    }
    if (rects.empty()) return;

    // A scroll frame damages two disjoint strips and must keep them separate
    // (their bounding box would be almost the whole window).
    if (rects.size() > 1) {
        RECT box = rects.front();
        long long area = 0;
        for (const RECT& rect : rects) {
            UnionRect(&box, &box, &rect);
            area += static_cast<long long>(rect.right - rect.left) *
                    static_cast<long long>(rect.bottom - rect.top);
        }
        const long long boxArea =
            static_cast<long long>(box.right - box.left) *
            static_cast<long long>(box.bottom - box.top);
        if (rects.size() > 16 || (boxArea > 0 && area * 4 >= boxArea * 3)) {
            rects.assign(1, box);
        }
    }

    const HDC previousMem = backBuffer_.Handle();
    if (!backBuffer_.Prepare(hdc, width_, height_)) {
        return;  // allocation failed; keep the previous frame
    }

    HDC mem = backBuffer_.Handle();
    if (mem != previousMem && renderer_) {
        renderer_->ResetBinding();
    }

    // Single-pass Direct2D rendering into the back buffer. Every damaged
    // rectangle is painted as its own clipped pass; the back buffer outside
    // them is left untouched (it is equally absent from Present() below).
    if (renderer_ && renderer_->AttachToHdc(mem, width_, height_)) {
        for (const RECT& rect : rects) {
            const BBox clip{static_cast<double>(rect.left),
                            static_cast<double>(rect.top),
                            static_cast<double>(rect.right),
                            static_cast<double>(rect.bottom)};
            renderer_->pushClip(clip);
            Path background;
            background.closed = true;
            background.points = {{clip.left, clip.top},
                                 {clip.right, clip.top},
                                 {clip.right, clip.bottom},
                                 {clip.left, clip.bottom}};
            renderer_->fillPath(background, BrushStyle{background_});
            painter(*renderer_, rect);
            renderer_->popClip();
        }
        renderer_->Detach();
    }

    // Blit the repainted rectangles to the target DC.
    for (const RECT& rect : rects) {
        backBuffer_.Present(hdc, &rect, destX, destY);
    }
}
