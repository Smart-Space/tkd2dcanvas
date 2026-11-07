"""M4 补充: polygon / arc items (create, options, coords)."""

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
        yield Canvas(root, width=320, height=240)
    except CommandError as exc:
        pytest.skip(f"native package not built yet: {exc}")
    finally:
        root.destroy()


def test_polygon_joinstyle_roundtrip(canvas):
    item = canvas.create_polygon(0, 0, 10, 0, 5, 10)
    assert canvas.itemcget(item, "joinstyle") == "round"
    canvas.itemconfigure(item, joinstyle="bevel")
    assert canvas.itemcget(item, "joinstyle") == "bevel"
    canvas.itemconfigure(item, joinstyle="miter")
    assert canvas.itemcget(item, "joinstyle") == "miter"


def test_line_joinstyle_roundtrip(canvas):
    item = canvas.create_line(0, 0, 10, 10)
    assert canvas.itemcget(item, "joinstyle") == "round"
    canvas.itemconfigure(item, joinstyle="miter")
    assert canvas.itemcget(item, "joinstyle") == "miter"


def test_line_capstyle_default_and_roundtrip(canvas):
    item = canvas.create_line(0, 0, 10, 10)
    assert canvas.itemcget(item, "capstyle") == "butt"
    canvas.itemconfigure(item, capstyle="projecting")
    assert canvas.itemcget(item, "capstyle") == "projecting"
    canvas.itemconfigure(item, capstyle="round")
    assert canvas.itemcget(item, "capstyle") == "round"


def test_capstyle_rejected_on_rectangle(canvas):
    rect = canvas.create_rectangle(0, 0, 10, 10)
    with pytest.raises(CommandError):
        canvas.itemconfigure(rect, capstyle="round")
    with pytest.raises(CommandError):
        canvas.itemcget(rect, "capstyle")


def test_joinstyle_rejected_on_rectangle(canvas):
    rect = canvas.create_rectangle(0, 0, 10, 10)
    with pytest.raises(CommandError):
        canvas.itemconfigure(rect, joinstyle="round")
    with pytest.raises(CommandError):
        canvas.itemcget(rect, "joinstyle")


def test_create_polygon(canvas):
    item = canvas.create_polygon(10, 10, 100, 10, 55, 90, fill="tomato")
    assert isinstance(item, int) and item > 0


def test_polygon_coords_roundtrip(canvas):
    item = canvas.create_polygon(0, 0, 50, 50, 100, 0)
    assert canvas.coords(item) == pytest.approx([0, 0, 50, 50, 100, 0])


def test_polygon_default_fill(canvas):
    item = canvas.create_polygon(0, 0, 10, 0, 5, 10)
    assert canvas.itemcget(item, "fill") == "#000000"
    assert canvas.itemcget(item, "outline") == ""


def test_polygon_bbox(canvas):
    item = canvas.create_polygon(20, 10, 100, 40, 40, 90)
    assert canvas.bbox(item) == pytest.approx((19, 9, 101, 91))


def test_create_arc(canvas):
    item = canvas.create_arc(0, 0, 100, 100)
    assert isinstance(item, int) and item > 0


def test_arc_defaults(canvas):
    item = canvas.create_arc(0, 0, 100, 100)
    assert canvas.itemcget(item, "start") == "0"
    assert canvas.itemcget(item, "extent") == "90"
    assert canvas.itemcget(item, "style") == "pieslice"


def test_arc_with_options(canvas):
    item = canvas.create_arc(10, 10, 110, 110, start=30, extent=120, style="chord")
    assert canvas.itemcget(item, "start") == "30"
    assert canvas.itemcget(item, "extent") == "120"
    assert canvas.itemcget(item, "style") == "chord"


def test_arc_itemconfigure(canvas):
    item = canvas.create_arc(0, 0, 100, 100)
    canvas.itemconfigure(item, start=45, extent=180, style="arc")
    assert canvas.itemcget(item, "start") == "45"
    assert canvas.itemcget(item, "extent") == "180"
    assert canvas.itemcget(item, "style") == "arc"


def test_arc_style_rejected_on_rectangle(canvas):
    rect = canvas.create_rectangle(0, 0, 10, 10)
    with pytest.raises(CommandError):
        canvas.itemconfigure(rect, start=45)
    # nothing was applied atomically
    with pytest.raises(CommandError):
        canvas.itemcget(rect, "start")
