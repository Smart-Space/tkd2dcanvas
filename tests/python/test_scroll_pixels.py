"""Pixel-level scroll tests.

A scroll frame shifts the retained back buffer and the window pixels with
ScrollWindowEx and repaints only the newly exposed strips. These tests verify
the resulting pixels, not just the view fractions: a wrongly signed shift or a
missed strip still reports the correct xview/yview.
"""

import ctypes
import sys
from ctypes import wintypes

import pytest

tkinter = pytest.importorskip("tkinter")

if sys.platform != "win32":
    pytest.skip("pixel checks use Win32 GDI", allow_module_level=True)

from tkd2dcanvas.errors import CommandError  # noqa: E402

user32 = ctypes.WinDLL("user32", use_last_error=True)
gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)

user32.GetDC.argtypes = [wintypes.HWND]
user32.GetDC.restype = wintypes.HDC
user32.ReleaseDC.argtypes = [wintypes.HWND, wintypes.HDC]
user32.ReleaseDC.restype = ctypes.c_int
gdi32.GetPixel.argtypes = [wintypes.HDC, ctypes.c_int, ctypes.c_int]
gdi32.GetPixel.restype = wintypes.DWORD

CLR_INVALID = 0xFFFFFFFF


def _colorref_to_rgb(pixel):
    return (pixel & 0xFF, (pixel >> 8) & 0xFF, (pixel >> 16) & 0xFF)


def _pixel(canvas, x, y):
    hwnd = int(canvas.master.tk.call("winfo", "id", str(canvas)), 0)
    hdc = user32.GetDC(hwnd)
    if not hdc:
        pytest.skip("could not acquire the canvas window DC")
    try:
        value = gdi32.GetPixel(hdc, x, y)
    finally:
        user32.ReleaseDC(hwnd, hdc)
    if value == CLR_INVALID:
        pytest.skip("GetPixel is not available for the canvas window")
    return _colorref_to_rgb(value)


@pytest.fixture(scope="module")
def root():
    # One root for both tests: creating several Tk roots in one process is
    # needlessly fragile (Tk library discovery).
    try:
        r = tkinter.Tk()
    except tkinter.TclError as exc:
        pytest.skip(f"Tk unavailable in this environment: {exc}")
    r.lift()
    try:
        yield r
    finally:
        r.destroy()


@pytest.fixture()
def canvas(root):
    from tkd2dcanvas import Canvas

    try:
        c = Canvas(root, width=200, height=100, background="white",
                   scrollregion=(0, 0, 500, 500))
    except CommandError as exc:
        pytest.skip(f"native package not built yet: {exc}")
    c.pack()
    root.lift()
    root.update()
    try:
        yield c
    finally:
        c.destroy()


def test_scroll_pixels_follow_origin(canvas):
    canvas.create_rectangle(10, 10, 60, 60, fill="#ff0000", outline="")
    canvas.create_rectangle(10, 110, 60, 160, fill="#0000ff", outline="")
    canvas.master.update()

    assert _pixel(canvas, 20, 20) == (255, 0, 0)
    assert _pixel(canvas, 20, 70) == (255, 255, 255)

    # One 50 px step down: the red box keeps only its last 10 px at the top,
    # the blue box enters at y=60.
    canvas.configure(yscrollincrement=50)
    canvas.yview("scroll", 1, "units")
    canvas.master.update()
    assert _pixel(canvas, 20, 5) == (255, 0, 0)
    assert _pixel(canvas, 20, 20) == (255, 255, 255)
    assert _pixel(canvas, 20, 70) == (0, 0, 255)

    # One 50 px step right: the green box moves from x=110 to x=60 and its
    # old position becomes background.
    canvas.create_rectangle(110, 20, 160, 60, fill="#00ff00", outline="")
    canvas.master.update()
    assert _pixel(canvas, 120, 5) == (0, 255, 0)

    canvas.configure(xscrollincrement=50)
    canvas.xview("scroll", 1, "units")
    canvas.master.update()
    assert _pixel(canvas, 70, 5) == (0, 255, 0)
    assert _pixel(canvas, 120, 5) == (255, 255, 255)

    # Diagonal scroll (two strips in one paint): the yellow box moves left
    # and up by 50 px.
    canvas.create_rectangle(130, 120, 180, 160, fill="#ffff00", outline="")
    canvas.master.update()
    assert _pixel(canvas, 100, 80) == (255, 255, 0)

    canvas.yview("scroll", 1, "units")
    canvas.xview("scroll", 1, "units")
    canvas.master.update()
    assert _pixel(canvas, 40, 30) == (255, 255, 0)


def test_edit_and_scroll_in_one_event_cycle(canvas):
    # Item damage that is pending when the scroll happens must move with the
    # content instead of being painted at its old window coordinates.
    canvas.configure(yscrollincrement=50)
    item = canvas.create_rectangle(10, 60, 60, 90, fill="#00ff00", outline="")
    canvas.master.update()
    assert _pixel(canvas, 20, 70) == (0, 255, 0)

    canvas.itemconfigure(item, fill="#ff8000")
    canvas.yview("scroll", 1, "units")  # no update in between
    canvas.master.update()

    # Canvas y=60..90 with origin 50 is window y=10..40; the recoloured item
    # is inside the retained area, so it is only correct when the pending
    # damage was carried over the scroll.
    assert _pixel(canvas, 20, 20) == (255, 128, 0)
    assert _pixel(canvas, 20, 70) == (255, 255, 255)
