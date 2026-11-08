"""Stage 9 (M10): window items.

The window item embeds a real Tk widget into the canvas (``-window``), with
``-width``/``-height`` sizing and ``-anchor`` placement. The placement of
the embedded window is derived from the item bbox and the view origin, so
it must follow the canvas content exactly when scrolling.
"""

import pytest

tkinter = pytest.importorskip("tkinter")

import sys

if sys.platform != "win32":
    pytest.skip("pixel checks use Win32 GDI", allow_module_level=True)

import ctypes  # noqa: E402
from ctypes import wintypes  # noqa: E402

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


@pytest.fixture()
def root():
    try:
        r = tkinter.Tk()
    except tkinter.TclError as exc:
        pytest.skip(f"Tk unavailable in this environment: {exc}")
    try:
        yield r
    finally:
        r.destroy()


@pytest.fixture()
def canvas(root):
    from tkd2dcanvas import Canvas

    try:
        c = Canvas(root, width=320, height=240, background="white",
                   scrollregion=(0, 0, 1000, 1000),
                   xscrollincrement=50, yscrollincrement=50)
    except CommandError as exc:
        pytest.skip(f"native package not built yet: {exc}")
    c.pack()
    try:
        yield c
    finally:
        c.destroy()


def _button(root, text):
    return tkinter.Button(root, text=text)


def test_create_window_returns_id_and_bbox(root, canvas):
    btn = _button(root, "OK")
    item = canvas.create_window(100, 100, window=btn, width=100, height=40)
    assert isinstance(item, int)
    assert canvas.bbox(item) == (50.0, 80.0, 150.0, 120.0)
    assert canvas.coords(item) == [100.0, 100.0]
    assert canvas.type(item) == "window"


def test_create_window_defaults_to_natural_size(root, canvas):
    btn = _button(root, "natural")
    root.update()
    item = canvas.create_window(100, 100, window=btn)
    x1, y1, x2, y2 = canvas.bbox(item)
    assert x2 - x1 == btn.winfo_reqwidth()
    assert y2 - y1 == btn.winfo_reqheight()
    # default anchor is center
    assert (x1 + x2) / 2 == pytest.approx(100.0, abs=0.6)
    assert (y1 + y2) / 2 == pytest.approx(100.0, abs=0.6)


def test_anchor_places_the_bbox(root, canvas):
    btn = _button(root, "anchor")
    item = canvas.create_window(100, 100, window=btn, width=100, height=40,
                                anchor="nw")
    assert canvas.bbox(item) == (100.0, 100.0, 200.0, 140.0)
    canvas.itemconfigure(item, anchor="se")
    assert canvas.bbox(item) == (0.0, 60.0, 100.0, 100.0)
    canvas.itemconfigure(item, anchor="center")
    assert canvas.bbox(item) == (50.0, 80.0, 150.0, 120.0)


def test_width_height_reconfigure_resizes_embedded(root, canvas):
    btn = _button(root, "size me")
    item = canvas.create_window(50, 50, window=btn, width=60, height=30)
    root.update()
    assert (btn.winfo_width(), btn.winfo_height()) == (60, 30)
    canvas.itemconfigure(item, width=200, height=77)
    root.update()
    assert (btn.winfo_width(), btn.winfo_height()) == (200, 77)
    canvas.itemconfigure(item, width=0, height=0)  # back to natural
    root.update()
    assert (btn.winfo_width(), btn.winfo_height()) == (
        btn.winfo_reqwidth(), btn.winfo_reqheight())


def test_itemcget_and_report_keys(root, canvas):
    btn = _button(root, "report")
    item = canvas.create_window(10, 10, window=btn, anchor="ne",
                                width=120, height=33)
    assert canvas.itemcget(item, "-window") == str(btn)
    assert canvas.itemcget(item, "-anchor") == "ne"
    assert canvas.itemcget(item, "-width") == "120"
    assert canvas.itemcget(item, "-height") == "33"
    report = canvas.itemconfigure(item)
    assert report["window"] == str(btn)
    assert report["width"] == "120"
    assert report["height"] == "33"
    assert report["anchor"] == "ne"


def test_window_must_exist_or_itemconfigure_fails(root, canvas):
    btn = _button(root, "ok")
    item = canvas.create_window(10, 10, window=btn)
    with pytest.raises(CommandError):
        canvas.create_window(10, 10, window=".nonexistent_window")
    # a failed -window reconfigure leaves the item (and the old binding) intact
    with pytest.raises(CommandError):
        canvas.itemconfigure(item, window=".definitely_not_here")
    assert canvas.itemcget(item, "-window") == str(btn)
    root.update()
    assert btn.winfo_ismapped() == 1


def test_toplevel_is_rejected(root, canvas):
    toplevel = tkinter.Toplevel(root)
    try:
        with pytest.raises(CommandError):
            canvas.create_window(10, 10, window=str(toplevel))
    finally:
        toplevel.destroy()


def test_canvas_itself_is_rejected(root, canvas):
    with pytest.raises(CommandError):
        canvas.create_window(10, 10, window=str(canvas))


def test_move_and_coords(root, canvas):
    btn = _button(root, "move me")
    item = canvas.create_window(10, 10, window=btn)
    canvas.move(item, 50, 40)
    assert canvas.coords(item) == [60.0, 50.0]
    assert canvas.bbox(item) is not None


def test_hidden_state_unmaps_and_reshows(root, canvas):
    btn = _button(root, "hide me")
    item = canvas.create_window(30, 30, window=btn, width=80, height=30)
    root.update()
    assert btn.winfo_ismapped() == 1
    canvas.itemconfigure(item, state="hidden")
    root.update()
    assert btn.winfo_ismapped() == 0
    assert canvas.bbox(item) is None
    canvas.itemconfigure(item, state="normal")
    root.update()
    assert btn.winfo_ismapped() == 1


def test_scroll_keeps_embedded_window_in_sync(root, canvas):
    btn = _button(root, "scroll with me")
    item = canvas.create_window(100, 100, window=btn, width=100, height=40)
    root.update()
    before = (btn.winfo_x(), btn.winfo_y())
    canvas.xview("scroll", 1, "units")
    canvas.yview("scroll", 1, "units")
    root.update()
    after = (btn.winfo_x(), btn.winfo_y())
    assert before[0] - after[0] == 50
    assert before[1] - after[1] == 50
    # bbox is measured in canvas coordinates, so it does not change
    assert canvas.bbox(item) == (50.0, 80.0, 150.0, 120.0)
    canvas.xview("scroll", -1, "units")
    canvas.yview("scroll", -1, "units")


def test_scroll_moves_embedded_window_with_content(root, canvas):
    btn = _button(root, "move")
    item = canvas.create_window(100, 100, window=btn, width=100, height=40)
    root.update()
    before = btn.winfo_y()
    canvas.yview("scroll", 2, "units")
    root.update()
    assert before - btn.winfo_y() == 100
    canvas.yview("scroll", -2, "units")


def test_scroll_far_away_unmaps_embedded_window(root, canvas):
    btn = _button(root, "off screen")
    item = canvas.create_window(50, 50, window=btn, width=100, height=40)
    root.update()
    assert btn.winfo_ismapped() == 1
    canvas.yview("moveto", 1.0)
    root.update()
    assert btn.winfo_ismapped() == 0
    canvas.yview("moveto", 0.0)
    root.update()
    assert btn.winfo_ismapped() == 1


def test_delete_unmaps_embedded_window(root, canvas):
    btn = _button(root, "delete me")
    item = canvas.create_window(20, 20, window=btn)
    root.update()
    assert btn.winfo_ismapped() == 1
    canvas.delete(item)
    root.update()
    assert btn.winfo_ismapped() == 0


def test_scale_scales_item_point_and_requested_size(root, canvas):
    btn = _button(root, "scale me")
    item = canvas.create_window(50, 50, window=btn, width=100, height=40)
    canvas.scale(item, 0, 0, 2.0, 0.5)
    assert canvas.coords(item) == [100.0, 25.0]
    assert canvas.itemcget(item, "-width") == "200"
    assert canvas.itemcget(item, "-height") == "20"


def test_find_overlapping_and_enclosed(root, canvas):
    btn = _button(root, "find me")
    item = canvas.create_window(100, 100, window=btn, width=100, height=40)
    canvas.update()
    assert item in canvas.find_overlapping(0, 0, 150, 150)
    assert item in canvas.find_enclosed(0, 0, 1000, 1000)


def test_frame_with_children_embedded(root, canvas):
    frame = tkinter.Frame(root)
    tkinter.Label(frame, text="a").pack()
    tkinter.Button(frame, text="b").pack()
    item = canvas.create_window(100, 100, window=frame)
    root.update()
    x1, y1, x2, y2 = canvas.bbox(item)
    assert (x2 - x1, y2 - y1) != (0, 0)
    canvas.move(item, 30, 20)
    assert canvas.coords(item) == [130.0, 120.0]


def test_create_window_item_while_scrolled(root, canvas):
    # Placing a new window item with origin != 0 must position the child
    # consistently with the canvas content (bbox is in canvas coordinates).
    btn = _button(root, "late")
    canvas.yview("scroll", 1, "units")  # origin 50
    item = canvas.create_window(100, 100, window=btn, width=80, height=30)
    root.update()
    assert canvas.bbox(item) == (60.0, 85.0, 140.0, 115.0)
    assert btn.winfo_y() == 85 - 50
    canvas.yview("scroll", -1, "units")


def test_window_item_above_graphics(root, canvas):
    # The window item is always drawn above graphical items (Tk limitation):
    # create graphics that fully overlap it.
    btn = _button(root, "on top")
    item = canvas.create_window(60, 60, window=btn, width=80, height=30)
    rect = canvas.create_rectangle(0, 0, 400, 400, fill="red")
    canvas.lift(rect)  # must not matter for window items
    root.update()
    assert btn.winfo_ismapped() == 1


def test_scroll_pixels_shift_keeps_window_in_sync(root, canvas):
    # A full red band covers the area the embedded window slides over; the
    # retained-buffer shift and the child reposition must agree pixel for
    # pixel (bbox/scroll consistency, M10).
    canvas.create_rectangle(0, 0, 400, 700, fill="#ff0000", outline="")
    btn = _button(root, "pixel sync")
    item = canvas.create_window(100, 100, window=btn, width=80, height=30)
    root.update()
    before_y = btn.winfo_y()
    assert _pixel(canvas, 20, 20) == (255, 0, 0)
    canvas.yview("scroll", 1, "units")  # origin 50
    root.update()
    assert _pixel(canvas, 20, 20) == (255, 0, 0)
    assert before_y - btn.winfo_y() == 50
    # scrolled so far down that the item leaves the view: it must be unmapped
    canvas.yview("scroll", 6, "units")  # origin 350
    root.update()
    assert btn.winfo_ismapped() == 0
    # scrolling back re-maps it at the right place
    canvas.yview("scroll", -6, "units")
    root.update()
    assert btn.winfo_ismapped() == 1
    assert btn.winfo_y() == 35
