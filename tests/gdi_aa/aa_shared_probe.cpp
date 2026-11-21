// aa_shared_probe.cpp -- the off-screen buffers of the anti-aliasing
// engine are shared by the whole process.
//
// Before the change every canvas widget had its own off-screen buffers
// ("number of widgets x 1 MB", the last item of the road map).  Now there
// is one set of buffers for the whole process, exactly like Tk does it on
// its side (tk/win/tkWinAA.c keeps the buffers in a state that all
// canvases of the thread share).
//
// This probe renders the same shape with several engines (one per canvas
// widget) and checks that
//
//   1. the process holds one set of off-screen buffers, not one per engine,
//   2. every engine renders the same pixels - the shared buffers are not
//      left dirty by the shape that was drawn before.

#include <windows.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "../../native/render/GdiAntialias.h"
#include "../../native/render/GdiScratchPool.h"

using tkd2dcanvas::AaPoint;

namespace {

constexpr int kWidth = 200;
constexpr int kHeight = 200;
constexpr int kEngines = 8;

HDC dc = nullptr;
HBITMAP bitmap = nullptr;
DWORD* pixels = nullptr;

int failures = 0;

void Check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

void CreateDestination() {
    BITMAPINFO info;
    std::memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = kWidth;
    info.bmiHeader.biHeight = -kHeight;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    HDC screen = GetDC(nullptr);
    dc = CreateCompatibleDC(screen);
    pixels = nullptr;
    bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS,
                              reinterpret_cast<void**>(&pixels), nullptr, 0);
    ReleaseDC(nullptr, screen);
    SelectObject(dc, bitmap);
}

void ClearDestination() {
    const DWORD white = 0xFFFFFFFFu;
    for (int i = 0; i < kWidth * kHeight; ++i) pixels[i] = white;
}

// A shape with a slanted edge: the anti-aliased edge is where a dirty
// buffer would show up first.
void BuildShape(std::vector<AaPoint>& out) {
    out.clear();
    for (int i = 0; i < 48; ++i) {
        const double a = 2.0 * 3.14159265358979323846 * i / 48.0;
        AaPoint p;
        p.x = 100.0 + 60.0 * std::cos(a);
        p.y = 100.0 + 45.0 * std::sin(a);
        out.push_back(p);
    }
}

}  // namespace

int main() {
    CreateDestination();

    std::vector<AaPoint> shape;
    BuildShape(shape);

    // The reference: the first engine renders the shape.
    ClearDestination();
    {
        tkd2dcanvas::GdiAntialias aa;
        aa.SetScale(4);
        aa.FillPolygon(dc, shape.data(), static_cast<int>(shape.size()),
                       Color(32, 96, 160));
    }
    std::vector<DWORD> reference(static_cast<size_t>(kWidth) * kHeight);
    std::memcpy(reference.data(), pixels, reference.size() * sizeof(DWORD));

    // Every other engine (every other canvas widget) renders the same
    // shape into the same destination: the pixels have to be identical.
    int mismatched = 0;
    for (int engine = 1; engine < kEngines; ++engine) {
        ClearDestination();
        tkd2dcanvas::GdiAntialias aa;
        aa.SetScale(4);
        aa.FillPolygon(dc, shape.data(), static_cast<int>(shape.size()),
                       Color(32, 96, 160));
        for (int i = 0; i < kWidth * kHeight; ++i) {
            if (pixels[i] != reference[static_cast<size_t>(i)]) ++mismatched;
        }
    }
    std::printf("1. %d engines render the same pixels: %d of %d differ\n",
                kEngines, mismatched, kWidth * kHeight);
    Check(mismatched == 0,
          "the shared off-screen buffers do not leak between the engines");

    // The buffers are shared: the process holds one set of them, no matter
    // how many engines (canvas widgets) there are.
    const long long shared = tkd2dcanvas::GdiAntialias::SharedScratchBytes();
    const long long budget = tkd2dcanvas::GdiAntialias::kScratchBudgetBytes;
    std::printf("2. shared off-screen buffers: %lld bytes (budget %lld bytes), "
                "%d buffers\n",
                shared, budget,
                tkd2dcanvas::GdiScratchPool::Instance().BufferCount());
    Check(shared <= budget,
          "the shared off-screen buffers stay inside the memory budget");
    Check(tkd2dcanvas::GdiScratchPool::Instance().BufferCount() == 2,
          "the process holds one scratch buffer and one read-back buffer");

    // Per engine the buffers are what they always were: inside the budget.
    tkd2dcanvas::GdiAntialias aa;
    aa.SetScale(4);
    ClearDestination();
    aa.FillPolygon(dc, shape.data(), static_cast<int>(shape.size()),
                   Color(32, 96, 160));
    std::printf("3. one engine uses %lld bytes of the shared buffers\n",
                aa.ScratchBytes());
    Check(aa.ScratchBytes() <= budget,
          "the buffers of a single shape stay inside the memory budget");

    SelectObject(dc, bitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);

    if (failures == 0) {
        std::printf("aa_shared_probe: all checks passed\n");
        return 0;
    }
    std::printf("aa_shared_probe: %d check(s) failed\n", failures);
    return 1;
}
