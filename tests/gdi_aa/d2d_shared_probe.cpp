// d2d_shared_probe.cpp -- the Direct2D render target is shared by the whole
// process.
//
// Every canvas widget used to own a render target of its own.  A render
// target comes with a device and a command queue, and Direct2D releases
// those resources asynchronously, so a widget that recreated its target
// (which happened on every resize) left the memory of the old target behind
// for a while.
//
// This probe paints with several renderers (one per canvas widget) and
// checks that
//
//   1. the process creates one render target, not one per widget,
//   2. a widget that gets a new back buffer (a resize) does not make the
//      process create another render target,
//   3. every widget gets its own picture (the shared target is bound to the
//      right back buffer).

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#include "../../native/render/D2DRenderer.h"
#include "../../native/render/D2DTargetPool.h"

namespace {

constexpr int kWidgets = 8;

int failures = 0;

void Check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

// A back buffer: what a canvas widget paints into.
struct BackBuffer {
    HDC dc = nullptr;
    HBITMAP bitmap = nullptr;
    DWORD* pixels = nullptr;
    int width = 0;
    int height = 0;

    bool Create(int w, int h) {
        width = w;
        height = h;
        BITMAPINFO info;
        std::memset(&info, 0, sizeof(info));
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = w;
        info.bmiHeader.biHeight = -h;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;

        HDC screen = GetDC(nullptr);
        dc = CreateCompatibleDC(screen);
        pixels = nullptr;
        bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS,
                                   reinterpret_cast<void**>(&pixels), nullptr, 0);
        ReleaseDC(nullptr, screen);
        if (!dc || !bitmap) return false;
        SelectObject(dc, bitmap);
        return true;
    }

    void Destroy() {
        if (dc) {
            if (bitmap) SelectObject(dc, bitmap);
            DeleteDC(dc);
            dc = nullptr;
        }
        if (bitmap) {
            DeleteObject(bitmap);
            bitmap = nullptr;
        }
    }
};

// Paints a rectangle into the back buffer of the widget.
void Paint(BackBuffer& buffer, const Color& color) {
    D2DRenderer renderer;
    if (!renderer.AttachToHdc(buffer.dc, buffer.width, buffer.height)) {
        std::fprintf(stderr, "FAIL: could not attach to the back buffer\n");
        ++failures;
        return;
    }
    renderer.clear(Color(255, 255, 255));

    Path rect;
    rect.closed = true;
    rect.points.push_back(Point{10.0, 10.0});
    rect.points.push_back(Point{60.0, 10.0});
    rect.points.push_back(Point{60.0, 40.0});
    rect.points.push_back(Point{10.0, 40.0});
    renderer.fillPath(rect, BrushStyle{color});

    renderer.Detach();
}

}  // namespace

int main() {
    const long long before = D2DTargetPool::TargetsCreated();

    // Every widget paints into its own back buffer.
    std::vector<std::unique_ptr<BackBuffer>> buffers;
    for (int i = 0; i < kWidgets; ++i) {
        auto buffer = std::make_unique<BackBuffer>();
        if (!buffer->Create(200, 120)) {
            std::fprintf(stderr, "FAIL: could not create the back buffer\n");
            return 1;
        }
        buffers.push_back(std::move(buffer));
    }

    for (int i = 0; i < kWidgets; ++i) {
        Paint(*buffers[static_cast<size_t>(i)], Color(200, 40, 60));
    }

    const long long created = D2DTargetPool::TargetsCreated() - before;
    std::printf("1. %d widgets painted, %lld render target(s) created\n",
                kWidgets, created);
    Check(created == 1,
          "the process creates one render target for all canvas widgets");

    // Every widget got its own picture.
    int wrong = 0;
    for (int i = 0; i < kWidgets; ++i) {
        const BackBuffer& buffer = *buffers[static_cast<size_t>(i)];
        // The rectangle is opaque red, the rest of the buffer is white.
        // A 32 bit DIB stores the pixels as B, G, R, X, so the DWORD reads
        // as 0xAARRGGBB.
        const DWORD inside = buffer.pixels[25 * buffer.width + 30];
        const DWORD outside = buffer.pixels[100 * buffer.width + 150];
        const bool insideOk = (inside & 0x00FFFFFF) == 0x00C8283C;
        const bool outsideOk = (outside & 0x00FFFFFF) == 0x00FFFFFF;
        if (!insideOk || !outsideOk) ++wrong;
    }
    std::printf("2. %d of %d back buffers have the expected picture\n", wrong,
                kWidgets);
    Check(wrong == 0, "every canvas widget paints into its own back buffer");

    // A widget that gets a new back buffer (a resize) must not make the
    // process create another render target: the shared target is bound to
    // the new back buffer instead.
    {
        BackBuffer resized;
        resized.Create(320, 200);
        Paint(resized, Color(10, 200, 90));
        const long long afterResize = D2DTargetPool::TargetsCreated() - before;
        std::printf("3. after a resize: %lld render target(s) created\n",
                    afterResize);
        Check(afterResize == 1,
              "a resize does not create another render target");
        const DWORD inside = resized.pixels[25 * resized.width + 30];
        Check((inside & 0x00FFFFFF) == 0x000AC85A,
              "the resized back buffer has the expected picture");
        resized.Destroy();
    }

    for (auto& buffer : buffers) buffer->Destroy();

    if (failures == 0) {
        std::printf("d2d_shared_probe: all checks passed\n");
        return 0;
    }
    std::printf("d2d_shared_probe: %d check(s) failed\n", failures);
    return 1;
}
