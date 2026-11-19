// aa_edge_probe.cpp -- 抗锯齿引擎的三个可量化特性。
//
// 这些不是"对错"问题，而是手动抗锯齿（GDI 没有自带抗锯齿）的固有性质，
// 写在这里是为了让它们可以被量化、被回归，而不是靠肉眼判断。
//
//   1. 裁剪安全性
//      画布是"按脏矩形逐块重绘"的，脏矩形的边界可能正好切在一条抗锯齿
//      边缘中间。引擎把整个形状渲染到离屏缓冲、再整块合成到目标，
//      所以裁剪只影响"哪些像素被写"，不影响"像素长什么样"。
//      => 裁剪过的可见像素必须和没裁剪时逐字节相同。
//
//   2. 覆盖率量化
//      N 倍超采样 = 每像素 N*N 个采样点 => 覆盖率只有 N*N+1 个离散值。
//      4 倍 -> 17 级（相邻灰阶差 16/255），8 倍 -> 65 级。
//      D2D 的 PER_PRIMITIVE 是解析覆盖率（256 级）。
//      这就是边缘看起来"有台阶"（画面割裂）的直接原因。
//
//   3. 相邻图元的接缝
//      两个图元各自独立做抗锯齿、再各自 source-over 合成时，共享边上
//      的两个半覆盖像素各叠一次，背景会漏进来一点（50%+50% -> 25%+25%）。
//      这条性质 D2D 也有（逐图元混合），不是 GDI 独有的。

#include <windows.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <vector>

#include "../../native/render/GdiAntialias.h"

using tkd2dcanvas::AaPoint;

namespace {

constexpr int kWidth = 200;
constexpr int kHeight = 200;

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

// An ellipse centred at (100, 100) with radii 60x40.
const AaPoint kOval[16] = {
    {160, 100}, {152.7, 116.7}, {138.6, 129.6}, {120.0, 137.1},
    {100.0, 140.0}, {80.0, 137.1}, {61.4, 129.6}, {47.3, 116.7},
    {40.0, 100.0}, {47.3, 83.3}, {61.4, 70.4}, {80.0, 62.9},
    {100.0, 60.0}, {120.0, 62.9}, {138.6, 70.4}, {152.7, 83.3},
};

void DrawOval() {
    tkd2dcanvas::GdiAntialias aa;
    aa.SetScale(4);
    aa.FillPolygon(dc, kOval, 16, Color(32, 96, 160));
}

// ---------------------------------------------------------------------------
// 1. 裁剪安全性
// ---------------------------------------------------------------------------
void TestClipSafety() {
    ClearDestination();
    DrawOval();
    std::vector<DWORD> reference(static_cast<size_t>(kWidth) * kHeight);
    std::memcpy(reference.data(), pixels, reference.size() * sizeof(DWORD));

    // The clip boundary crosses the right edge of the oval.
    ClearDestination();
    IntersectClipRect(dc, 0, 0, 130, kHeight);
    DrawOval();
    SelectClipRgn(dc, nullptr);

    int mismatched = 0;
    int firstX = -1, firstY = -1;
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < 130; ++x) {
            if (pixels[y * kWidth + x] != reference[y * kWidth + x]) {
                if (mismatched == 0) {
                    firstX = x;
                    firstY = y;
                }
                ++mismatched;
            }
        }
    }
    std::printf("1. clip safety: %d of %d visible pixels differ", mismatched,
                130 * kHeight);
    if (mismatched) {
        std::printf(" (first at %d,%d)", firstX, firstY);
    }
    std::printf("\n");
    Check(mismatched == 0,
          "the anti-aliased edge survives a clip region unchanged");
}

// ---------------------------------------------------------------------------
// 2. 覆盖率量化
// ---------------------------------------------------------------------------
int CountLevels(int scale) {
    ClearDestination();
    tkd2dcanvas::GdiAntialias aa;
    aa.SetScale(scale);

    // A shallow diagonal: the coverage changes along its length.
    const AaPoint line[2] = {{0, 48}, {kWidth, 80}};
    aa.StrokePolyline(dc, line, 2, false, Color(0, 0, 0), PenStyle());

    std::set<int> levels;
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const DWORD p = pixels[y * kWidth + x];
            const int v = (static_cast<int>((p >> 16) & 0xFF) +
                           static_cast<int>((p >> 8) & 0xFF) +
                           static_cast<int>(p & 0xFF)) / 3;
            if (v > 0 && v < 255) levels.insert(v);
        }
    }
    return static_cast<int>(levels.size());
}

void TestCoverageQuantization() {
    const int levels4 = CountLevels(4);
    const int levels8 = CountLevels(8);
    std::printf("2. coverage quantization: %d gray levels at scale 4, "
                "%d at scale 8 (D2D: analytic, 256)\n",
                levels4, levels8);
    // The step size halves when the sample count quadruples, so the number
    // of distinguishable gray levels has to grow.
    Check(levels8 > levels4, "a higher supersampling factor gives more gray levels");
}

// ---------------------------------------------------------------------------
// 3. 相邻图元的接缝
// ---------------------------------------------------------------------------
void TestSharedEdgeSeam() {
    tkd2dcanvas::GdiAntialias aa;
    aa.SetScale(4);
    const DWORD blue = 0xFFFF0000u;  // BGRA: pure blue

    // Reference: one rectangle whose right edge is at x = 32.5.
    ClearDestination();
    {
        const AaPoint rect[4] = {{16, 16}, {32.5, 16}, {32.5, 48}, {16, 48}};
        aa.FillPolygon(dc, rect, 4, Color(0, 0, 255));
    }
    const DWORD whole = pixels[32 * kWidth + 32];

    // Two rectangles that share that edge, each covering half of pixel 32.
    ClearDestination();
    {
        const AaPoint left[4] = {{16, 16}, {32.5, 16}, {32.5, 48}, {16, 48}};
        aa.FillPolygon(dc, left, 4, Color(0, 0, 255));
        const AaPoint right[4] = {{32.5, 16}, {48, 16}, {48, 48}, {32.5, 48}};
        aa.FillPolygon(dc, right, 4, Color(0, 0, 255));
    }
    const DWORD split = pixels[32 * kWidth + 32];

    std::printf("3. shared edge: one shape %08X, two shapes %08X "
                "(white = FFFFFFFF)\n", whole, split);
    // One shape: half covered -> 50% blue over white.
    // Two shapes: 50% then 50% again -> 25% blue over white (background
    // leaked in).  This is inherent to per-shape compositing and is the
    // reason adjacent items can show a faint seam.
    Check(whole != split, "two shapes sharing an edge are composited separately");
    (void)blue;
}

}  // namespace

int main() {
    CreateDestination();

    TestClipSafety();
    TestCoverageQuantization();
    TestSharedEdgeSeam();

    SelectObject(dc, bitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);

    if (failures == 0) {
        std::printf("aa_edge_probe: all checks passed\n");
        return 0;
    }
    std::printf("aa_edge_probe: %d check(s) failed\n", failures);
    return 1;
}
