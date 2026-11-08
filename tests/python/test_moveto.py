"""moveto + Tk-compatible line bbox (the bbox is intentionally *not* the
tight visual hull; Tk inflates it to integer units by half the pen width
plus a 1px fudge — see ComputeLineBbox in tkCanvLine.c)."""

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


def test_line_bbox_is_the_tk_inflated_box(canvas):
    item = canvas.create_line(10, 10, 50, 30)
    assert canvas.bbox(item) == pytest.approx((8, 8, 52, 32))


def test_line_bbox_width(canvas):
    item = canvas.create_line(10, 10, 50, 30, width=2)
    assert canvas.bbox(item) == pytest.approx((7, 7, 53, 33))
    canvas.itemconfigure(item, width=4.4)
    assert canvas.bbox(item) == pytest.approx((5, 5, 55, 35))
    canvas.itemconfigure(item, width=4.5)  # rounds half up to 5
    assert canvas.bbox(item) == pytest.approx((4, 4, 56, 36))


def test_line_bbox_miter_apex(canvas):
    item = canvas.create_line(0, 0, 100, 0, 10, 20, width=20, joinstyle="miter")
    miter_box = canvas.bbox(item)
    canvas.itemconfigure(item, joinstyle="round")
    round_box = canvas.bbox(item)
    assert miter_box == pytest.approx((-21, -21, 192, 41))
    assert round_box == pytest.approx((-21, -21, 121, 41))


def test_line_bbox_smooth_does_not_expand(canvas):
    item = canvas.create_line(0, 0, 50, 100, 100, 0, smooth=True)
    assert canvas.bbox(item) == pytest.approx((-2, -2, 102, 102))


def test_moveto_aligns_bbox_origin(canvas):
    item = canvas.create_line(10, 10, 50, 30)
    canvas.moveto(item, 100, 100)
    assert canvas.coords(item) == pytest.approx([102, 102, 142, 122])
    assert canvas.bbox(item) == pytest.approx((100, 100, 144, 124))


def test_moveto_reference_is_int_bbox_not_coords(canvas):
    item = canvas.create_line(10.7, 10.2, 50, 30)
    canvas.moveto(item, 100, 100)
    assert canvas.coords(item) == pytest.approx([102.7, 102.2, 142, 122])
    assert canvas.bbox(item) == pytest.approx((100, 100, 144, 124))


def test_moveto_empty_string_keeps_axis(canvas):
    item = canvas.create_line(10, 10, 50, 30)
    canvas.moveto(item, "", 200)
    assert canvas.coords(item) == pytest.approx([10, 202, 50, 222])
    canvas.moveto(item, 300, "")
    assert canvas.coords(item) == pytest.approx([302, 202, 342, 222])


def test_moveto_all_matching_items_share_the_first_delta(canvas):
    a = canvas.create_line(10, 10, 50, 30, tags="grp")
    b = canvas.create_line(20, 20, 60, 40, tags="grp")
    canvas.moveto("grp", 100, 100)
    assert canvas.coords(a) == pytest.approx([102, 102, 142, 122])
    assert canvas.coords(b) == pytest.approx([112, 112, 152, 132])


def test_moveto_unknown_tag_is_a_noop(canvas):
    canvas.moveto("no-such-tag", 10, 10)


def test_find_overlapping_uses_stroke_not_bbox(canvas):
    # Tk asks the line's areaProc; the inflated bbox only acts as a coarse
    # 1px-padded filter. A probe overlapping only the bbox pad — but not the
    # stroke — must not report the line.
    item = canvas.create_line(20, 20, 100, 20, width=2, tags="stroke")
    assert canvas.find_overlapping(16, 16, 19, 19) == ()
    assert canvas.find_overlapping(18, 22, 18, 23) == ()
    assert canvas.find_overlapping(18, 19, 30, 21) == (item,)
    assert canvas.find_enclosed(0, 0, 200, 200) == (item,)
    assert canvas.find_enclosed(0, 0, 50, 18) == ()
