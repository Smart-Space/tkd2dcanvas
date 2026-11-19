// aa_rect_equiv.cpp -- 像素对齐的矩形：抗锯齿引擎 vs 普通 FillRect。
//
// 这是"GDI 后端对像素对齐矩形走快路径"的正确性依据：
//
// 一个轴对齐、边落在整数像素上的矩形**没有抗锯齿边缘** ——
// 每个像素要么被完全覆盖，要么完全没被覆盖。抗锯齿引擎把它放大 N 倍
// 画进离屏缓冲、再逐采样点统计覆盖率，降采样之后得到的像素和直接
// FillRect 完全一样（RGB 逐字节相同）。
//
// 所以这种矩形可以绕过抗锯齿引擎 —— 代价相差 250 倍：
// 600x400 的背景矩形，抗锯齿引擎 4.0 ms，FillRect 0.016 ms。
//
// 本测试守住这个等价关系：任何改动都不能让两者画出不同的 RGB。

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <vector>

#include "../../native/render/GdiAntialias.h"

using tkd2dcanvas::AaPoint;

namespace {

constexpr int kWidth = 200;
constexpr int kHeight = 200;

struct Surface {
    HDC dc = nullptr;
    HBITMAP bmp = nullptr;
    DWORD* px = nullptr;
};

Surface Make() {
    Surface s;
    BITMAPINFO info;
    std::memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = kWidth;
    info.bmiHeader.biHeight = -kHeight;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    HDC screen = GetDC(nullptr);
    s.dc = CreateCompatibleDC(screen);
    void* bits = nullptr;
    s.bmp = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, screen);
    SelectObject(s.dc, s.bmp);
    s.px = static_cast<DWORD*>(bits);
    return s;
}

void Clear(Surface& s) {
    for (int i = 0; i < kWidth * kHeight; ++i) s.px[i] = 0xFFFFFFFFu;
}

// Only the colour channels matter: the alpha byte of a 32 bit DIB is
// unused (the back buffer is blitted with BitBlt).
inline bool SameRgb(DWORD a, DWORD b) {
    return (a & 0x00FFFFFFu) == (b & 0x00FFFFFFu);
}

}  // namespace

int main() {
    Surface s = Make();
    int failures = 0;

    struct Rect { int l, t, r, b; };
    const Rect rects[] = {
        {10, 10, 40, 30},     // even / even
        {11, 13, 47, 39},     // odd / odd
        {0, 0, 200, 200},     // the whole surface
        {64, 64, 65, 65},     // a single pixel
        {10, 20, 190, 21},    // a single row
        {37, 41, 38, 42},     // two pixels
    };

    for (const Rect& r : rects) {
        tkd2dcanvas::GdiAntialias aa;
        aa.SetScale(4);

        // (a) through the anti-aliasing engine
        Clear(s);
        const AaPoint poly[4] = {{static_cast<double>(r.l), static_cast<double>(r.t)},
                                  {static_cast<double>(r.r), static_cast<double>(r.t)},
                                  {static_cast<double>(r.r), static_cast<double>(r.b)},
                                  {static_cast<double>(r.l), static_cast<double>(r.b)}};
        aa.FillPolygon(s.dc, poly, 4, Color(10, 20, 30));
        std::vector<DWORD> viaAa(static_cast<size_t>(kWidth) * kHeight);
        std::memcpy(viaAa.data(), s.px, viaAa.size() * sizeof(DWORD));

        // (b) with a plain FillRect
        Clear(s);
        RECT rc{r.l, r.t, r.r, r.b};
        HBRUSH brush = CreateSolidBrush(RGB(10, 20, 30));
        FillRect(s.dc, &rc, brush);
        DeleteObject(brush);

        int diff = 0;
        int firstX = -1, firstY = -1;
        for (int i = 0; i < kWidth * kHeight; ++i) {
            if (!SameRgb(viaAa[static_cast<size_t>(i)], s.px[i])) {
                if (diff == 0) {
                    firstX = i % kWidth;
                    firstY = i / kWidth;
                }
                ++diff;
            }
        }
        std::printf("rect (%3d,%3d)-(%3d,%3d): %d pixels differ", r.l, r.t, r.r, r.b,
                    diff);
        if (diff) {
            std::printf(" (first at %d,%d)", firstX, firstY);
        }
        std::printf("\n");
        if (diff) ++failures;
    }

    SelectObject(s.dc, s.bmp);
    DeleteObject(s.bmp);
    DeleteDC(s.dc);

    if (failures == 0) {
        std::printf("aa_rect_equiv: all checks passed\n");
        return 0;
    }
    std::printf("aa_rect_equiv: %d rectangle(s) differ\n", failures);
    return 1;
}
