"""M9: text / image / bitmap items."""

import ctypes
import sys
from ctypes import wintypes

import pytest

tkinter = pytest.importorskip("tkinter")

from tkd2dcanvas.errors import CommandError  # noqa: E402


@pytest.fixture()
def canvas():
    try:
        root = tkinter.Tk()
    except tkinter.TclError as exc:
        pytest.skip(f"Tk unavailable in this environment: {exc}")
    from tkd2dcanvas import Canvas

    try:
        yield Canvas(root, width=320, height=240, background="white")
    except CommandError as exc:
        pytest.skip(f"native package not built yet: {exc}")
    finally:
        root.destroy()


# --- text --------------------------------------------------------------------

def test_create_text(canvas):
    item = canvas.create_text(10, 10, text="hello")
    assert isinstance(item, int) and item > 0


def test_text_coords_roundtrip(canvas):
    item = canvas.create_text(11, 22, text="hello")
    assert canvas.coords(item) == pytest.approx([11, 22])
    canvas.coords(item, 33, 44)
    assert canvas.coords(item) == pytest.approx([33, 44])


def test_text_default_font_and_anchor(canvas):
    item = canvas.create_text(10, 10, text="hello")
    assert canvas.itemcget(item, "font")  # non-empty default
    assert canvas.itemcget(item, "anchor") == "center"
    assert canvas.itemcget(item, "justify") == "left"
    assert canvas.itemcget(item, "wrap") == "word"
    assert canvas.itemcget(item, "width") == "0"
    assert canvas.itemcget(item, "fill") == "#000000"


def test_text_full_options(canvas):
    item = canvas.create_text(40, 40, text="one\ntwo\nthree",
                              font=("Helvetica", 14, "bold"), anchor="nw",
                              justify="right", width=120, fill="navy")
    assert canvas.itemcget(item, "text") == "one\ntwo\nthree"
    assert canvas.itemcget(item, "font") == "Helvetica 14 bold"
    assert canvas.itemcget(item, "anchor") == "nw"
    assert canvas.itemcget(item, "justify") == "right"
    assert canvas.itemcget(item, "width") == "120"
    assert canvas.itemcget(item, "fill") == "#000080"


def test_text_bad_anchor_raises(canvas):
    with pytest.raises(CommandError):
        canvas.create_text(0, 0, text="hi", anchor="sideways")


def test_text_bad_justify_raises(canvas):
    with pytest.raises(CommandError):
        canvas.create_text(0, 0, text="hi", justify="ragged")


def test_text_bad_wrap_raises(canvas):
    with pytest.raises(CommandError):
        canvas.create_text(0, 0, text="hi", wrap="syllables")


def test_text_empty_font_raises(canvas):
    with pytest.raises(CommandError):
        canvas.create_text(0, 0, text="hi", font="")


def test_text_bad_font_size_raises(canvas):
    with pytest.raises(CommandError):
        canvas.create_text(0, 0, text="hi", font=("Arial", "bogus"))


def test_text_image_option_rejected(canvas):
    photo = _make_photo(canvas, "opt_rejected", 1, 1)
    with pytest.raises(CommandError):
        canvas.create_text(0, 0, text="hi", image=photo)


def test_text_outline_option_rejected(canvas):
    with pytest.raises(CommandError):
        canvas.create_text(0, 0, text="hi", outline="red")


def test_text_angle_roundtrip(canvas):
    item = canvas.create_text(20, 20, text="abc", anchor="nw", angle=45)
    assert canvas.itemcget(item, "angle") == "45"


def test_text_angle_normalized_negative(canvas):
    item = canvas.create_text(0, 0, text="abc", angle=-45)
    assert canvas.itemcget(item, "angle") == "315"


def test_text_angle_normalized_over_360(canvas):
    item = canvas.create_text(0, 0, text="abc", angle=405)
    assert canvas.itemcget(item, "angle") == "45"


def test_text_angle_default_is_zero(canvas):
    item = canvas.create_text(0, 0, text="abc")
    assert canvas.itemcget(item, "angle") == "0"


def test_text_angle_widens_rotated_bbox(canvas):
    flat = canvas.create_text(100, 100, text="HHHHHHHH", anchor="center", angle=0)
    rotated = canvas.create_text(100, 100, text="HHHHHHHH", anchor="center", angle=45)
    flat_h = canvas.bbox(flat)[3] - canvas.bbox(flat)[1]
    rot_h = canvas.bbox(rotated)[3] - canvas.bbox(rotated)[1]
    rot_w = canvas.bbox(rotated)[2] - canvas.bbox(rotated)[0]
    flat_w = canvas.bbox(flat)[2] - canvas.bbox(flat)[0]
    # A wide block rotated 45 degrees gets taller and narrower.
    assert rot_h > flat_h
    assert 0 < rot_w < flat_w


def test_text_angle_itemconfigure(canvas):
    item = canvas.create_text(0, 0, text="abc", angle=0)
    canvas.itemconfigure(item, angle=400)
    assert canvas.itemcget(item, "angle") == "40"


def test_text_angle_rejected_on_rectangle(canvas):
    rect = canvas.create_rectangle(0, 0, 10, 10)
    with pytest.raises(CommandError):
        canvas.itemconfigure(rect, angle=45)


def test_text_width_grows_bbox_height(canvas):
    item = canvas.create_text(0, 0, text="alpha beta gamma delta epsilon zeta eta theta",
                              font=("Arial", 20), width=80, anchor="nw")
    box = canvas.bbox(item)
    assert box is not None
    assert (box[3] - box[1]) > 26


def test_text_move(canvas):
    item = canvas.create_text(10, 10, text="hello")
    canvas.move(item, 15, 25)
    assert canvas.coords(item) == pytest.approx([25, 35])


def test_text_itemconfigure_roundtrip(canvas):
    item = canvas.create_text(0, 0, text="hello", anchor="nw")
    canvas.itemconfigure(item, anchor="se", justify="center", wrap="char")
    assert canvas.itemcget(item, "anchor") == "se"
    assert canvas.itemcget(item, "justify") == "center"
    assert canvas.itemcget(item, "wrap") == "char"


def test_text_itemconfigure_atomic_failure(canvas):
    item = canvas.create_text(0, 0, text="hello")
    with pytest.raises(CommandError):
        canvas.itemconfigure(item, justify="center", anchor="diagonal")
    assert canvas.itemcget(item, "justify") == "left"
    assert canvas.itemcget(item, "anchor") == "center"


# --- image -------------------------------------------------------------------

def _make_photo(canvas, name, width, height):
    photo = tkinter.PhotoImage(master=canvas.master, name=name,
                               width=width, height=height)
    photo.put("#ff0000", to=(0, 0, width - 1, height - 1))
    return photo


def test_create_image(canvas):
    photo = _make_photo(canvas, "py_image_1", 24, 10)
    item = canvas.create_image(30, 40, image=photo, anchor="nw")
    assert isinstance(item, int) and item > 0
    assert canvas.bbox(item) == pytest.approx((30, 40, 54, 50))


def test_image_default_anchor_centers(canvas):
    photo = _make_photo(canvas, "py_image_2", 24, 10)
    item = canvas.create_image(30, 40, image=photo)
    assert canvas.bbox(item) == pytest.approx((18, 35, 42, 45))


def test_image_empty_create(canvas):
    item = canvas.create_image(30, 40)
    assert canvas.bbox(item) is None


def test_image_bad_name_raises(canvas):
    with pytest.raises(CommandError):
        canvas.create_image(0, 0, image="no_such_photo_xyz")


def test_image_reconfigure(canvas):
    small = _make_photo(canvas, "py_image_3", 2, 2)
    big = _make_photo(canvas, "py_image_4", 7, 5)
    item = canvas.create_image(10, 10, image=small, anchor="nw")
    canvas.itemconfigure(item, image=big)
    assert canvas.bbox(item) == pytest.approx((10, 10, 17, 15))
    assert canvas.itemcget(item, "image") == "py_image_4"


def test_image_reconfigure_bad_photo_aborts(canvas):
    photo = _make_photo(canvas, "py_image_5", 2, 2)
    item = canvas.create_image(10, 10, image=photo, anchor="nw")
    with pytest.raises(CommandError):
        canvas.itemconfigure(item, anchor="se", image="no_such_photo_zzz")
    assert canvas.itemcget(item, "anchor") == "nw"


def test_image_coords(canvas):
    photo = _make_photo(canvas, "py_image_6", 2, 2)
    item = canvas.create_image(11, 22, image=photo)
    assert canvas.coords(item) == pytest.approx([11, 22])


def test_image_option_on_rect_rejected(canvas):
    photo = _make_photo(canvas, "py_image_7", 1, 1)
    with pytest.raises(CommandError):
        canvas.create_rectangle(0, 0, 10, 10, image=photo)


# --- bitmap ------------------------------------------------------------------

def test_create_bitmap_builtin(canvas):
    item = canvas.create_bitmap(10, 10, bitmap="info", anchor="nw")
    assert isinstance(item, int) and item > 0
    assert canvas.bbox(item) == pytest.approx((10, 10, 18, 31))


def test_bitmap_unknown_name_raises(canvas):
    with pytest.raises(CommandError):
        canvas.create_bitmap(0, 0, bitmap="no_such_bitmap_xyz")


def test_bitmap_at_file(canvas, tmp_path):
    xbm = (
        "#define testbit_width 5\n"
        "#define testbit_height 3\n"
        "static unsigned char testbit_bits[] = {\n"
        "   0x15, 0x0a, 0x15};\n"
    )
    path = tmp_path / "testbit.xbm"
    path.write_text(xbm)
    item = canvas.create_bitmap(0, 0, bitmap=f"@{path}", anchor="nw")
    assert canvas.bbox(item) == pytest.approx((0, 0, 5, 3))


def test_bitmap_foreground_background_roundtrip(canvas):
    item = canvas.create_bitmap(0, 0, bitmap="info",
                                foreground="purple", background="yellow")
    assert canvas.itemcget(item, "foreground") == "#800080"
    assert canvas.itemcget(item, "background") == "#ffff00"
    canvas.itemconfigure(item, background="")
    assert canvas.itemcget(item, "background") == ""


def test_bitmap_option_rejected_on_rect(canvas):
    with pytest.raises(CommandError):
        canvas.create_rectangle(0, 0, 10, 10, bitmap="info")


# --- pixel-level rendering check ----------------------------------------------

if sys.platform == "win32":
    user32 = ctypes.WinDLL("user32", use_last_error=True)
    gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)
    user32.GetDC.argtypes = [wintypes.HWND]
    user32.GetDC.restype = wintypes.HDC
    user32.ReleaseDC.argtypes = [wintypes.HWND, wintypes.HDC]
    user32.ReleaseDC.restype = ctypes.c_int
    gdi32.GetPixel.argtypes = [wintypes.HDC, ctypes.c_int, ctypes.c_int]
    gdi32.GetPixel.restype = wintypes.DWORD


def _pixel(canvas, x, y):
    hwnd = int(canvas.master.tk.call("winfo", "id", str(canvas)), 0)
    hdc = user32.GetDC(hwnd)
    if not hdc:
        pytest.skip("could not acquire the canvas window DC")
    try:
        value = gdi32.GetPixel(hdc, x, y)
    finally:
        user32.ReleaseDC(hwnd, hdc)
    if value == 0xFFFFFFFF:
        pytest.skip("GetPixel is not available for the canvas window")
    return (value & 0xFF, (value >> 8) & 0xFF, (value >> 16) & 0xFF)


@pytest.mark.skipif(sys.platform != "win32", reason="pixel checks use Win32 GDI")
def test_image_item_paints_pixels(canvas):
    photo = _make_photo(canvas, "pix_check", 16, 16)
    canvas.create_image(10, 10, image=photo, anchor="nw")
    canvas.master.update()
    red, green, blue = _pixel(canvas, 18, 18)
    assert red > 200 and green < 60 and blue < 60


@pytest.mark.skipif(sys.platform != "win32", reason="pixel checks use Win32 GDI")
def test_text_item_paints_pixels(canvas):
    canvas.create_text(10, 10, text="Ag", font=("Helvetica", 24, "bold"),
                       fill="black", anchor="nw")
    canvas.master.update()
    dark = 0
    for x in range(10, 50):
        for y in range(10, 40):
            r, g, b = _pixel(canvas, x, y)
            if r + g + b < 200:
                dark += 1
    assert dark > 0
