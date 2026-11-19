// aa_render_test.cpp -- unit test for the manual anti-aliasing engine.
//
// The engine renders a shape into an off-screen buffer, filters it down and
// composites the result onto the destination.  Two mistakes are invisible in
// a screenshot but break the rendering completely:
//
//   * the BLENDFUNCTION of the AlphaBlend call has to use AC_SRC_ALPHA as
//     AlphaFormat - with any other value AlphaBlend fails with
//     ERROR_INVALID_PARAMETER and the shape is not drawn at all,
//   * a 32 bit DIB stores its pixels as B, G, R, A, so the color has to be
//     written in that order (red and blue would be swapped otherwise).
//
// The test therefore checks the pixels that come out of the engine.
//
// 鐢ㄦ硶:  aa_render_test.exe

#include <windows.h>

#include <cstdio>
#include <cstdlib>

#include "../../native/render/GdiAntialias.h"

using tkd2dcanvas::AaPoint;

namespace {

constexpr int kWidth = 64;
constexpr int kHeight = 64;

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

// Returns the pixel at (x, y) as 0xAARRGGBB.
DWORD Pixel(int x, int y) {
    return pixels[y * kWidth + x];
}

void CreateDestination() {
    BITMAPINFO info;
    std::memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = kWidth;
    info.bmiHeader.biHeight = -kHeight;  // top-down
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    HDC screen = GetDC(nullptr);
    dc = CreateCompatibleDC(screen);
    pixels = nullptr;
    bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS,
                              reinterpret_cast<void**>(&pixels), nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (!bitmap) {
        std::fprintf(stderr, "CreateDIBSection failed\n");
        std::exit(1);
    }
    SelectObject(dc, bitmap);
}

void ClearDestination() {
    DWORD white = 0xFFFFFFFFu;
    for (int i = 0; i < kWidth * kHeight; ++i) pixels[i] = white;
}

// A filled axis aligned rectangle: the interior has to be the exact color.
void TestFilledRectangle() {
    ClearDestination();
    tkd2dcanvas::GdiAntialias aa;
    aa.SetScale(4);

    const AaPoint points[4] = {{16, 16}, {48, 16}, {48, 48}, {16, 48}};
    const bool ok = aa.FillPolygon(dc, points, 4, Color(224, 144, 144));
    Check(ok, "FillPolygon reports success (AlphaBlend must not fail)");

    // The interior is fully covered: the color has to be the requested one.
    // (Red and blue would be swapped if the DIB byte order was wrong.)
    // A 32 bit DIB stores its pixels as B, G, R, A.
    const DWORD expected = 0xFF000000u | (224u << 16) | (144u << 8) | 144u;
    Check(Pixel(32, 32) == expected, "the interior has the requested color");
    Check(Pixel(20, 20) == expected, "the interior near the corner matches");

    // The area outside the rectangle must not be touched.
    Check(Pixel(4, 4) == 0xFFFFFFFFu, "the area outside stays white");
    Check(Pixel(60, 60) == 0xFFFFFFFFu, "the area outside stays white (2)");
}

// A diagonal edge has to be smoothed: the pixels on the edge are a mix of
// the shape color and the background.
void TestAntiAliasedEdge() {
    ClearDestination();
    tkd2dcanvas::GdiAntialias aa;
    aa.SetScale(4);

    // A right triangle with a hypotenuse from (8, 8) to (56, 56).
    const AaPoint points[3] = {{8, 8}, {56, 8}, {8, 56}};
    const bool ok = aa.FillPolygon(dc, points, 3, Color(0, 0, 0));
    Check(ok, "the triangle is drawn");

    // Count the partially covered pixels along the hypotenuse: without the
    // anti-aliasing the edge would be a hard staircase.
    int partial = 0;
    for (int i = 0; i < 48; ++i) {
        for (int dy = -1; dy <= 1; ++dy) {
            const DWORD p = Pixel(8 + i, 56 - i + dy);
            const int red = static_cast<int>((p >> 16) & 0xFF);
            if (red > 20 && red < 235) ++partial;
        }
    }
    Check(partial > 20, "the diagonal edge is anti-aliased");
}

// A stroke is drawn in the pen color and stays inside its bounding box.
void TestStroke() {
    ClearDestination();
    tkd2dcanvas::GdiAntialias aa;
    aa.SetScale(4);

    const AaPoint points[2] = {{16, 32}, {48, 32}};
    PenStyle pen;
    pen.color = Color(32, 64, 128);
    pen.width = 4.0;
    const bool ok = aa.StrokePolyline(dc, points, 2, false, pen.color, pen);
    Check(ok, "StrokePolyline reports success");

    const DWORD expected = 0xFF000000u | (32u << 16) | (64u << 8) | 128u;
    Check(Pixel(32, 32) == expected, "the stroke has the pen color");
    Check(Pixel(32, 20) == 0xFFFFFFFFu, "nothing is drawn above the line");
}

// An odd-width round-capped stroke: the two halves of each cap have to be
// covered the same way (GDI's PS_ENDCAP_ROUND is not), and the straight
// part has to stay crisp.
void TestRoundCap() {
    ClearDestination();
    tkd2dcanvas::GdiAntialias aa;
    aa.SetScale(4);

    // The AlignStrokePath-aligned position of a 3px line at y = 20.5.
    const AaPoint points[2] = {{16, 20.5}, {48, 20.5}};
    PenStyle pen;
    pen.color = Color(0, 0, 0);
    pen.width = 3.0;
    pen.cap = CapStyle::Round;
    const bool ok = aa.StrokePolyline(dc, points, 2, false, pen.color, pen);
    Check(ok, "the round cap stroke is drawn");

    // The straight part covers the same rows fully, like Tk and Direct2D do.
    Check(Pixel(32, 19) == 0xFF000000u, "the top body row is fully covered");
    Check(Pixel(32, 20) == 0xFF000000u, "the middle body row is fully covered");
    Check(Pixel(32, 21) == 0xFF000000u, "the bottom body row is fully covered");
    Check(Pixel(32, 18) == 0xFFFFFFFFu, "nothing is drawn above the stroke");
    Check(Pixel(32, 22) == 0xFFFFFFFFu, "nothing is drawn below the stroke");

    // The cap is symmetric about the stroke axis.
    Check(Pixel(15, 19) == Pixel(15, 21), "the cap is symmetric (1)");
    Check(Pixel(16, 19) == Pixel(16, 21), "the cap is symmetric (2)");
    Check(Pixel(14, 19) == Pixel(14, 21), "the cap is symmetric (3)");
    Check(Pixel(14, 20) != 0xFFFFFFFFu, "the cap reaches past the line end");
    Check(Pixel(13, 20) == 0xFFFFFFFFu, "nothing is drawn beyond the tip");
}

// A 1px stroke that sits on a half pixel has to cover exactly one pixel
// row (like Tk and Direct2D do), not two rows with 50% each.
void TestOnePixelStroke() {
    ClearDestination();
    tkd2dcanvas::GdiAntialias aa;
    aa.SetScale(4);

    // The stroke edges of a 1px wide line at y = 32.5 are at 32 and 33, so
    // the line covers exactly the pixel row 32.
    const AaPoint points[2] = {{16, 32.5}, {48, 32.5}};
    PenStyle pen;
    pen.color = Color(0, 0, 0);
    pen.width = 1.0;
    const bool ok = aa.StrokePolyline(dc, points, 2, false, pen.color, pen);
    Check(ok, "the 1px stroke is drawn");

    Check(Pixel(32, 32) == 0xFF000000u, "the stroke covers the pixel row 32");
    Check(Pixel(32, 31) == 0xFFFFFFFFu, "the row above stays untouched");
    Check(Pixel(32, 33) == 0xFFFFFFFFu, "the row below stays untouched");
}

}  // namespace

int main() {
    CreateDestination();

    TestFilledRectangle();
    TestAntiAliasedEdge();
    TestStroke();
    TestOnePixelStroke();
    TestRoundCap();

    SelectObject(dc, bitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);

    if (failures == 0) {
        std::printf("aa_render_test: all checks passed\n");
        return 0;
    }
    std::printf("aa_render_test: %d check(s) failed\n", failures);
    return 1;
}

