"""Scroll command tests (milestone M7)."""

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
        c = Canvas(root, width=200, height=100,
                   scrollregion=(0, 0, 1000, 500))
        c.pack()
        root.update()
        yield c
    except CommandError as exc:
        pytest.skip(f"native package not built yet (M2 pending): {exc}")
    finally:
        root.destroy()


def test_xview_default(canvas):
    assert canvas.xview() == pytest.approx((0.0, 0.2))


def test_yview_default(canvas):
    assert canvas.yview() == pytest.approx((0.0, 0.2))


def test_xview_moveto(canvas):
    canvas.xview("moveto", 0.5)
    assert canvas.xview() == pytest.approx((0.5, 0.7))


def test_xview_moveto_confine(canvas):
    canvas.xview("moveto", 1.0)
    assert canvas.xview() == pytest.approx((0.8, 1.0))
    canvas.xview("moveto", -0.5)
    assert canvas.xview() == pytest.approx((0.0, 0.2))


def test_yview_moveto(canvas):
    canvas.yview("moveto", 0.5)
    assert canvas.yview() == pytest.approx((0.5, 0.7))
    canvas.yview("moveto", 1.0)
    assert canvas.yview() == pytest.approx((0.8, 1.0))


def test_xview_scroll_units_without_increment(canvas):
    # Tk: one tenth of the visible width per unit.
    canvas.xview("moveto", 0.0)
    canvas.xview("scroll", 2, "units")
    assert canvas.xview()[0] == pytest.approx(0.04)


def test_xview_scroll_pages(canvas):
    canvas.xview("moveto", 0.0)
    canvas.xview("scroll", 1, "pages")
    # One page is nine tenths of the visible width (180 canvas units).
    assert canvas.xview()[0] == pytest.approx(0.18)


def test_xview_scroll_negative(canvas):
    canvas.xview("moveto", 0.5)
    canvas.xview("scroll", -2, "units")
    assert canvas.xview()[0] == pytest.approx(0.46)


def test_xscrollincrement_rounds_the_origin(canvas):
    canvas.configure(xscrollincrement=100)
    canvas.xview("moveto", 0.13)
    assert canvas.xview()[0] == pytest.approx(0.1)
    canvas.xview("scroll", 2, "units")
    assert canvas.xview()[0] == pytest.approx(0.3)


def test_yscrollincrement_rounds_the_origin(canvas):
    canvas.configure(yscrollincrement=50)
    canvas.yview("moveto", 0.13)
    assert canvas.yview()[0] == pytest.approx(0.1)


def test_xscrollincrement_units(canvas):
    canvas.configure(xscrollincrement=100)
    canvas.xview("moveto", 0.0)
    canvas.xview("scroll", 1, "units")
    assert canvas.xview()[0] == pytest.approx(0.1)


def test_confine_off_allows_moving_past_the_region(canvas):
    canvas.configure(confine=False)
    canvas.xview("moveto", 2.0)
    assert canvas.xview()[0] == pytest.approx(2.0)
    canvas.configure(confine=True)
    assert canvas.xview()[0] == pytest.approx(0.8)


def test_bad_scrollregion_configure_raises(canvas):
    with pytest.raises(CommandError):
        canvas.configure(scrollregion="1 2 3")


def test_bad_scrollregion_leaves_scrolling_disabled(canvas):
    with pytest.raises(CommandError):
        canvas.configure(scrollregion="1 2 3")
    # Tk drops the region on error: the full span is reported.
    assert canvas.xview() == pytest.approx((0.0, 1.0))


def test_bad_scrollregion_at_creation_raises():
    try:
        root = tkinter.Tk()
    except tkinter.TclError as exc:
        pytest.skip(f"Tk unavailable in this environment: {exc}")
    try:
        from tkd2dcanvas import Canvas

        with pytest.raises(CommandError):
            Canvas(root, width=200, height=100, scrollregion=(0, 0, 10))
    finally:
        root.destroy()


def test_scan_mark_dragto(canvas):
    canvas.xview("moveto", 0.5)
    canvas.scan_mark(100, 50)
    canvas.scan_dragto(90, 50)
    assert canvas.xview()[0] == pytest.approx(0.6)
    canvas.scan_mark(100, 50)
    canvas.scan_dragto(110, 50)
    assert canvas.xview()[0] == pytest.approx(0.5)


def test_scan_dragto_custom_gain(canvas):
    canvas.xview("moveto", 0.5)
    canvas.scan_mark(100, 50)
    canvas.scan_dragto(90, 50, gain=1)
    assert canvas.xview()[0] == pytest.approx(0.51)


def test_scroll_command_receives_fractions(canvas):
    # Mirrors examples/python/scrolling.py: a tkinter Scrollbar wired via a
    # Python-level bound method at creation time.
    vbar = tkinter.Scrollbar(canvas.master, orient="vertical")
    hbar = tkinter.Scrollbar(canvas.master, orient="horizontal")
    from tkd2dcanvas import Canvas

    try:
        c = Canvas(canvas.master, width=200, height=100,
                   scrollregion=(0, 0, 1000, 500),
                   xscrollcommand=hbar.set,
                   yscrollcommand=vbar.set)
        c.pack()
        canvas.master.update()
        assert tuple(float(v) for v in canvas.master.tk.eval(f"{vbar} get").split()) == pytest.approx((0.0, 0.2))
        c.yview("moveto", 0.5)
        canvas.master.update()
        assert tuple(float(v) for v in canvas.master.tk.eval(f"{vbar} get").split()) == pytest.approx((0.5, 0.7))
    finally:
        vbar.destroy()
        hbar.destroy()
