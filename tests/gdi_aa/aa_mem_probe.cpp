// aa_mem_probe.cpp -- how much memory does the anti-aliasing engine use
// when it renders the same shapes over and over?
//
// The canvas grows its private commit while it repaints, although the module
// has no live allocations left at the end.  This probe runs the engine on
// its own (no Tk, no Python) and prints the process' private commit, so that
// the engine can be blamed or cleared.
//
// Usage:  aa_mem_probe.exe [frames] [shapes]

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <windows.h>
#include <psapi.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

#include "../../native/render/GdiAntialias.h"

using tkd2dcanvas::AaPoint;

namespace {

int failures = 0;

HDC dc = nullptr;
HBITMAP bitmap = nullptr;
DWORD* pixels = nullptr;
int kWidth = 800;
int kHeight = 600;

struct MemInfo {
    SIZE_T privateBytes;
    SIZE_T workingSet;
};

MemInfo Mem() {
    PROCESS_MEMORY_COUNTERS_EX counters;
    std::memset(&counters, 0, sizeof(counters));
    counters.cb = sizeof(counters);
    GetProcessMemoryInfo(GetCurrentProcess(),
                         reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                         sizeof(counters));
    MemInfo info;
    info.privateBytes = counters.PrivateUsage;
    info.workingSet = counters.WorkingSetSize;
    return info;
}

// The CRT heap of this process (the dynamic CRT keeps one heap per process).
// HeapSize() faults on this SDK, so the heap is not queried directly.
SIZE_T CrtHeapBytes() { return 0; }

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
    bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS,
                              reinterpret_cast<void**>(&pixels), nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (!bitmap) {
        std::fprintf(stderr, "CreateDIBSection failed\n");
        std::exit(1);
    }
    SelectObject(dc, bitmap);
}

std::vector<AaPoint> OvalPath(double cx, double cy, double rx, double ry) {
    std::vector<AaPoint> points;
    points.reserve(64);
    constexpr double kTwoPi = 6.28318530717958647692;
    for (int i = 0; i < 64; ++i) {
        const double angle = kTwoPi * static_cast<double>(i) / 64.0;
        AaPoint p;
        p.x = cx + rx * std::cos(angle);
        p.y = cy + ry * std::sin(angle);
        points.push_back(p);
    }
    return points;
}

}  // namespace

int main(int argc, char** argv) {
    const int frames = argc > 1 ? std::atoi(argv[1]) : 400;
    const int shapes = argc > 2 ? std::atoi(argv[2]) : 30;
    // The canvas paints every damaged rectangle with a clip region set on
    // the destination DC (Win32Surface::Paint pushes the clip).
    const bool clip = argc > 3 ? std::atoi(argv[3]) != 0 : false;
    // Number of unrelated allocations per frame (mimics the rest of the app).
    const int noise = argc > 4 ? std::atoi(argv[4]) : 0;

    CreateDestination();

    tkd2dcanvas::GdiAntialias aa;
    aa.SetScale(4);

    // The shapes of the canvas: ovals with a fill and an outline.
    std::vector<std::vector<AaPoint>> paths;
    for (int i = 0; i < shapes; ++i) {
        const double x = 20.0 + (i % 10) * 70.0;
        const double y = 40.0 + (i / 10) * 250.0;
        paths.push_back(OvalPath(x + 30.0, y + 30.0, 30.0, 30.0));
    }

    const MemInfo base = Mem();
    const SIZE_T baseHeap = CrtHeapBytes();
    std::printf("start priv=%.0f KB crtHeap=%.0f KB\n",
                base.privateBytes / 1024.0, baseHeap / 1024.0);

    for (int frame = 1; frame <= frames; ++frame) {
        DWORD white = 0xFFFFFFFFu;
        for (int i = 0; i < kWidth * kHeight; ++i) pixels[i] = white;

        if (clip) {
            IntersectClipRect(dc, 0, 0, kWidth, kHeight);
        }

        for (int i = 0; i < shapes; ++i) {
            const std::vector<AaPoint>& path = paths[static_cast<size_t>(i)];
            aa.FillPolygon(dc, path.data(), static_cast<int>(path.size()),
                           Color(64, 128, 192));
            PenStyle pen;
            pen.width = 1.0;
            pen.color = Color(0, 0, 0);
            aa.StrokePolyline(dc, path.data(), static_cast<int>(path.size()),
                              true, pen.color, pen);
        }
        // The canvas builds a vector of (item, bbox) for every paint.
        std::vector<std::pair<void*, BBox>> items;
        items.reserve(static_cast<size_t>(shapes));
        for (int i = 0; i < shapes; ++i) {
            items.emplace_back(nullptr, BBox::Empty());
        }

        if (clip) {
            SelectClipRgn(dc, nullptr);
        }

        // The renderer is not the only thing that allocates: in the real
        // canvas the item list, the Tcl calls and the renderer interleave.
        // This noise mimics that.
        for (int n = 0; n < noise; ++n) {
            std::vector<char> junk(64 + (n * 37) % 4096);
            junk[0] = 1;
            std::vector<double> junk2((n * 13) % 300);
            if (!junk2.empty()) junk2[0] = 1.0;
        }

        if (frame % 50 == 0 || frame == 1) {
            const MemInfo now = Mem();
            const SIZE_T heap = CrtHeapBytes();
            std::printf("frame %5d priv=%8.0f KB d=%+9.0f KB  crtHeap=%8.0f KB "
                        "d=%+9.0f KB\n",
                        frame, now.privateBytes / 1024.0,
                        (now.privateBytes - base.privateBytes) / 1024.0,
                        heap / 1024.0, (heap - baseHeap) / 1024.0);
        }
    }

    if (failures) {
        std::printf("%d failures\n", failures);
        return 1;
    }
    return 0;
}

