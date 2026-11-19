/*
 * aa_engine_test.c --
 *
 *	Stand-alone test for the manual anti-aliasing engine of the Windows
 *	GDI back end (tk/win/tkWinAA.c).  The test draws a circle with the
 *	engine into an off-screen bitmap and prints the coverage that the
 *	engine computed for every pixel, so that the behaviour can be
 *	checked without a window.
 *
 *	Build and run it with:
 *
 *	    pwsh -File tests\gdi_aa\build_engine_test.ps1 -Run
 */

#include "tkWinInt.h"
#include "tkWinAA.h"
#include <stdio.h>
#include <stdlib.h>

#define WIDTH	128
#define HEIGHT	128

static HDC	dc;		/* Destination device context. */
static HBITMAP	bitmap;		/* Destination bitmap. */
static DWORD	*pixels;	/* Destination pixels. */
static int	failures = 0;

static void
CreateDestination(void)
{
    BITMAPINFO info;
    HDC screen;

    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = WIDTH;
    info.bmiHeader.biHeight = -HEIGHT;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    screen = GetDC(NULL);
    dc = CreateCompatibleDC(screen);
    pixels = NULL;
    bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS,
	    (void **) &pixels, NULL, 0);
    ReleaseDC(NULL, screen);
    if (bitmap == NULL) {
	fprintf(stderr, "CreateDIBSection failed\n");
	exit(1);
    }
    SelectObject(dc, bitmap);
    memset(pixels, 0xFF, WIDTH * HEIGHT * sizeof(DWORD));
}

static int
RedAt(int x, int y)
{
    /*
     * The background is white and the circle is red, so the amount of
     * red in a pixel is 255 minus its green component.
     */

    return 255 - (int) ((pixels[y * WIDTH + x] >> 8) & 0xFF);
}

/*
 * Draws a filled circle of radius 40 centered at (64, 64) with the
 * anti-aliasing engine.
 */

static void
DrawAntiAliasedCircle(int scale)
{
    int left = 64 - 44;
    int top = 64 - 44;
    int width = 88;
    int height = 88;
    HDC aaDC;
    HBRUSH brush, oldBrush;
    HPEN pen, oldPen;

    TkWinAAEnter(scale);
    aaDC = TkWinAABegin(dc, left, top, width, height);
    if (aaDC == NULL) {
	fprintf(stderr, "TkWinAABegin failed\n");
	exit(1);
    }
    pen = CreatePen(PS_SOLID, 1, RGB(1, 2, 3));
    oldPen = SelectObject(aaDC, pen);
    brush = CreateSolidBrush(RGB(1, 2, 3));
    oldBrush = SelectObject(aaDC, brush);
    Ellipse(aaDC, (24 - left) * scale, (24 - top) * scale,
	    (104 - left) * scale, (104 - top) * scale);
    SelectObject(aaDC, oldPen);
    SelectObject(aaDC, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
    TkWinAAEnd(dc, left, top, width, height, RGB(255, 0, 0));
    TkWinAALeave();
}

static void
Check(int condition, const char *what)
{
    if (!condition) {
	fprintf(stderr, "FAIL: %s\n", what);
	failures++;
    }
}

int
main(void)
{
    int x, y;
    int partial = 0;
    int full = 0;
    int empty = 0;

    CreateDestination();
    DrawAntiAliasedCircle(4);

    for (y = 0; y < HEIGHT; y++) {
	for (x = 0; x < WIDTH; x++) {
	    int red = RedAt(x, y);

	    if (red == 0) {
		empty++;
	    } else if (red == 255) {
		full++;
	    } else {
		partial++;
	    }
	}
    }
    printf("empty=%d partial=%d full=%d\n", empty, partial, full);
    Check(full > 3000, "the interior of the circle is filled");
    Check(partial > 100, "the edge of the circle is anti-aliased");
    Check(empty > 3000, "the background is untouched");
    Check(RedAt(64, 64) == 255, "the centre of the circle is red");

    /*
     * Print the coverage of the top left quarter of the circle, so that
     * the anti-aliased edge can be inspected.
     */

    for (y = 18; y < 40; y++) {
	for (x = 18; x < 60; x++) {
	    int red = RedAt(x, y);

	    if (red == 0) {
		putchar('.');
	    } else if (red == 255) {
		putchar('#');
	    } else {
		putchar("0123456789"[red * 10 / 256]);
	    }
	}
	putchar('\n');
    }

    /*
     * Write the result as a PPM file, so that it can be converted into
     * a PNG for a visual check.
     */

    {
	FILE *f = fopen("aa_engine_test.ppm", "wb");

	if (f != NULL) {
	    int i;

	    fprintf(f, "P6\n%d %d\n255\n", WIDTH, HEIGHT);
	    for (i = 0; i < WIDTH * HEIGHT; i++) {
		DWORD p = pixels[i];
		unsigned char rgb[3];

		rgb[0] = (unsigned char) ((p >> 16) & 0xFF);
		rgb[1] = (unsigned char) ((p >> 8) & 0xFF);
		rgb[2] = (unsigned char) (p & 0xFF);
		fwrite(rgb, 1, 3, f);
	    }
	    fclose(f);
	    printf("wrote aa_engine_test.ppm\n");
	}
    }

    if (failures) {
	printf("%d check(s) failed\n", failures);
	return 1;
    }
    printf("all checks passed\n");
    return 0;
}
