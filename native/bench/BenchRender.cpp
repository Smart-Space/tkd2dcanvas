// BenchRender.cpp - micro-benchmark for the Direct2D rendering backend.
// Simulates a WM_PAINT pass: clear background + draw N anti-aliased
// primitives onto a memory DC (the same path Win32Surface uses on real WM_PAINT).
//
// Build (from repo root, VS x64 Native Tools):
//   cmake -S . -B build/bench -DTKG_BUILD_BENCH=ON -DTKG_SYNC_PYTHON_PACKAGE=OFF
//   cmake --build build/bench --config Release
// or see docs/performance.md for a standalone cl command.

#include <windows.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <utility>
#include <vector>

#include "D2DRenderer.h"
#include "D2DContext.h"
#include "tkd2dcanvas/Geometry.h"
#include "tkd2dcanvas/BBox.h"

namespace {

constexpr int kWidth = 1280;
constexpr int kHeight = 800;
constexpr int kReps = 30;

struct BenchItem {
    // 0 = rect, 1 = ellipse(polygon), 2 = polyline
    int kind;
    std::vector<Point> geometry;
    Color fill;
    Color outline;
    BBox box;
};

std::vector<BenchItem> MakeItems(int count, std::uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> ux(0.0, static_cast<double>(kWidth));
    std::uniform_real_distribution<double> uy(0.0, static_cast<double>(kHeight));
    std::uniform_real_distribution<double> us(20.0, 140.0);

    std::vector<BenchItem> items;
    items.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        BenchItem item{};
        item.kind = i % 3;
        item.fill = Color{static_cast<std::uint8_t>(80 + (i * 37) % 170),
                          static_cast<std::uint8_t>(90 + (i * 53) % 150),
                          static_cast<std::uint8_t>(120 + (i * 29) % 125),
                          255};
        item.outline = Color{0, 0, 0, 255};

        if (item.kind == 0) {  // rect
            const double x = ux(rng), y = uy(rng), w = us(rng), h = us(rng);
            item.geometry = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
        } else if (item.kind == 1) {  // ellipse approx. polygon (48 segments)
            const double cx = ux(rng), cy = uy(rng);
            const double rx = us(rng) * 0.5, ry = us(rng) * 0.5;
            constexpr int kSegments = 48;
            for (int s = 0; s < kSegments; ++s) {
                const double a = 6.28318530718 * static_cast<double>(s) / kSegments;
                item.geometry.push_back({cx + rx * std::cos(a), cy + ry * std::sin(a)});
            }
        } else {  // polyline
            double x = ux(rng), y = uy(rng);
            item.geometry.push_back({x, y});
            for (int s = 0; s < 3; ++s) {
                x += us(rng);
                y += (static_cast<double>(rng() % 200) - 100.0);
                item.geometry.push_back({x, y});
            }
        }

        item.box = BBox{1.0e300, 1.0e300, -1.0e300, -1.0e300};
        for (const Point& point : item.geometry) {
            item.box.left = std::min(item.box.left, point.x);
            item.box.top = std::min(item.box.top, point.y);
            item.box.right = std::max(item.box.right, point.x);
            item.box.bottom = std::max(item.box.bottom, point.y);
        }
        items.push_back(std::move(item));
    }
    return items;
}

Path AsPath(const BenchItem& item) {
    Path p;
    p.points = item.geometry;
    p.closed = (item.kind != 2);
    return p;
}

void DrawOne(D2DRenderer& renderer, const BenchItem& item) {
    const Path path = AsPath(item);
    if (item.kind == 2) {
        PenStyle pen;
        pen.color = item.outline;
        pen.width = 2.0;
        renderer.drawLine(path, pen);
    } else {
        BrushStyle brush{item.fill};
        PenStyle pen;
        pen.color = item.outline;
        pen.width = 1.0;
        renderer.drawPath(path, pen, &brush);
    }
}

void DrawItems(D2DRenderer& renderer, const std::vector<BenchItem>& items) {
    for (const auto& item : items) DrawOne(renderer, item);
}

// The culling CanvasWidget::Paint does for a single damaged rectangle.
void DrawItemsCulled(D2DRenderer& renderer, const std::vector<BenchItem>& items,
                     const BBox& visible) {
    for (const auto& item : items) {
        if (item.box.IsValid() && !item.box.Intersects(visible)) continue;
        DrawOne(renderer, item);
    }
}

void FillRect(D2DRenderer& renderer, const BBox& box, const Color& color) {
    Path path;
    path.closed = true;
    path.points = {{box.left, box.top}, {box.right, box.top},
                   {box.right, box.bottom}, {box.left, box.bottom}};
    renderer.fillPath(path, BrushStyle{color});
}

double NowMs() {
    const auto now = std::chrono::steady_clock::now();
    return static_cast<double>(now.time_since_epoch().count()) /
           static_cast<double>(std::chrono::steady_clock::period::den) * 1000.0;
}

}  // namespace

int main() {
    if (!D2DContext::EnsureInitialized()) {
        std::puts("Direct2D failed to start");
        return 1;
    }

    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);

    // ID2D1DCRenderTarget::BindDC requires a 32bpp DIB section (top-down).
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = kWidth;
    bmi.bmiHeader.biHeight = -kHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp) {
        std::puts("failed to create 32bpp DIB section");
        ReleaseDC(nullptr, screen);
        return 1;
    }
    HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(mem, bmp));

    // One-time warmup (D2D loads its rasterizer on first draws).
    {
        D2DRenderer warm;
        warm.AttachToHdc(mem, kWidth, kHeight);
        warm.clear(Color{255, 255, 255});
        warm.Detach();
    }

    std::printf("%-8s %12s %12s\n", "items", "ms/frame", "us/item");
    for (const int count : {10, 50, 100, 200, 500, 1000}) {
        const auto items = MakeItems(count, 42);

        double best = 1e30;
        double total = 0.0;
        for (int rep = 0; rep < kReps; ++rep) {
            const double t0 = NowMs();

            D2DRenderer renderer;
            renderer.AttachToHdc(mem, kWidth, kHeight);
            renderer.clear(Color{255, 255, 255});
            DrawItems(renderer, items);
            renderer.Detach();

            const double dt = NowMs() - t0;
            best = dt < best ? dt : best;
            total += dt;
        }

        const double avg = total / kReps;
        std::printf("%-8d %12.2f %12.2f\n", count, avg, avg * 1000.0 / count);
    }

    // Reference: full-canvas clear cost alone (fresh renderer per rep).
    {
        const double t0 = NowMs();
        D2DRenderer renderer;
        renderer.AttachToHdc(mem, kWidth, kHeight);
        renderer.clear(Color{240, 240, 240});
        renderer.Detach();
        std::printf("clear-only (fresh renderer): %.2f ms\n", NowMs() - t0);
    }

    // Reference: frame lifecycle alone (BindDC + BeginDraw/EndDraw).
    {
        D2DRenderer renderer;
        double total = 0.0;
        for (int rep = 0; rep < kReps; ++rep) {
            const double t0 = NowMs();
            renderer.AttachToHdc(mem, kWidth, kHeight);
            renderer.Detach();
            total += NowMs() - t0;
        }
        std::printf("attach/detach only (persistent renderer): %.2f ms\n", total / kReps);
    }

    // Steady state: persistent renderer reused across frames (the real
    // Win32Surface pattern): only BindDC + Begin/EndDraw per frame.
    std::printf("persistent renderer (same DC):\n");
    for (const int count : {100, 500, 1000}) {
        const auto items = MakeItems(count, 42);
        D2DRenderer renderer;
        renderer.AttachToHdc(mem, kWidth, kHeight);  // creates rt once

        double total = 0.0;
        for (int rep = 0; rep < kReps; ++rep) {
            const double t0 = NowMs();

            renderer.AttachToHdc(mem, kWidth, kHeight);  // reuses rt
            renderer.clear(Color{255, 255, 255});
            DrawItems(renderer, items);
            renderer.Detach();

            total += NowMs() - t0;
        }

        renderer.Detach();
        const double avg = total / kReps;
        std::printf("%-8d %12.2f %12.2f\n", count, avg, avg * 1000.0 / count);
    }

    // Scroll frame: full window repaint vs only the newly exposed 20 px
    // strip (the path used by CanvasWidget::RepaintAfterScroll). Both frames
    // draw with origin shifted by 20 canvas units.
    constexpr int kStrip = 20;
    const BBox band{0.0, 380.0, static_cast<double>(kWidth), 380.0 + kStrip};
    std::printf("\nscroll frame (origin shifted by %d, %d px strip):\n", kStrip, kStrip);
    std::printf("%-8s %12s %12s %8s\n", "items", "full ms", "strip ms", "ratio");
    for (const int count : {100, 500, 1000}) {
        const auto items = MakeItems(count, 42);
        D2DRenderer renderer;
        renderer.AttachToHdc(mem, kWidth, kHeight);

        // Warm up the rasterizer / caches on this render target.
        renderer.clear(Color{255, 255, 255});
        DrawItems(renderer, items);
        renderer.Detach();

        double fullTotal = 0.0;
        double stripTotal = 0.0;
        for (int rep = 0; rep < kReps; ++rep) {
            const double fullStart = NowMs();
            renderer.AttachToHdc(mem, kWidth, kHeight);
            renderer.clear(Color{255, 255, 255});
            renderer.pushTranslate(0.0, -static_cast<double>(kStrip));
            DrawItems(renderer, items);
            renderer.popTranslate();
            renderer.Detach();
            fullTotal += NowMs() - fullStart;

            const double stripStart = NowMs();
            renderer.AttachToHdc(mem, kWidth, kHeight);
            renderer.pushClip(band);
            FillRect(renderer, band, Color{255, 255, 255});
            renderer.pushTranslate(0.0, -static_cast<double>(kStrip));
            const BBox visible{band.left, band.top + kStrip, band.right, band.bottom + kStrip};
            DrawItemsCulled(renderer, items, visible);
            renderer.popTranslate();
            renderer.popClip();
            renderer.Detach();
            stripTotal += NowMs() - stripStart;
        }

        renderer.Detach();
        const double fullAvg = fullTotal / kReps;
        const double stripAvg = stripTotal / kReps;
        std::printf("%-8d %12.2f %12.2f %7.1f%%\n", count, fullAvg, stripAvg,
                    fullAvg > 0.0 ? 100.0 * stripAvg / fullAvg : 0.0);
    }

    SelectObject(mem, oldBitmap);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    return 0;
}
