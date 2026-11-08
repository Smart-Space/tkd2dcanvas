"""scale / canvasx / canvasy / type / focus (milestone M8).

Semantics are copied from the Tk canvas (verified against the real canvas in
tests/python/test_tk_parity.py); the tests here pin them at the Python
level.
"""

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
        c = Canvas(root, width=320, height=240,
                   highlightthickness=0, borderwidth=0)
    except CommandError as exc:
        pytest.skip(f"native package not built yet: {exc}")
    yield c
    root.destroy()


# --- scale -------------------------------------------------------------------
def test_scale_doubles_coords(canvas):
    item = canvas.create_rectangle(10, 20, 110, 80)
    canvas.scale(item, 0, 0, 2, 3)
    assert canvas.coords(item) == pytest.approx([20, 60, 220, 240])


def test_scale_about_origin_and_negative(canvas):
    item = canvas.create_rectangle(10, 20, 110, 80)
    canvas.scale(item, 100, 100, -1, -1)
    # Tk normalizes rectangles to (left, top, right, bottom).
    assert canvas.coords(item) == pytest.approx([90, 120, 190, 180])


def test_scale_line_and_polygon(canvas):
    line = canvas.create_line(10, 10, 100, 50)
    canvas.scale(line, 0, 0, 0.5, 2)
    assert canvas.coords(line) == pytest.approx([5, 20, 50, 100])

    poly = canvas.create_polygon(0, 0, 40, 0, 20, 30)
    canvas.scale(poly, 10, 10, 2, 2)
    assert canvas.coords(poly) == pytest.approx([-10, -10, 70, -10, 30, 50])


def test_scale_arc(canvas):
    arc = canvas.create_arc(10, 10, 110, 90, start=30, extent=120)
    canvas.scale(arc, 60, 50, 2, 2)
    assert canvas.coords(arc) == pytest.approx([-40, -30, 160, 130])


def test_scale_all_and_empty_tag_is_noop(canvas):
    line = canvas.create_line(10, 10, 50, 50)
    canvas.scale("all", 0, 0, 1, 1)
    assert canvas.coords(line) == pytest.approx([10, 10, 50, 50])
    canvas.scale("no-such-tag", 0, 0, 2, 2)  # Tk: silent no-op
    assert canvas.coords(line) == pytest.approx([10, 10, 50, 50])


def test_scale_bad_factor_raises(canvas):
    item = canvas.create_line(0, 0, 10, 10)
    with pytest.raises(CommandError):
        canvas.scale(item, 0, 0, "not-a-number", 1)


# --- canvasx / canvasy ---------------------------------------------------------
def test_canvasx_maps_through_view_origin(canvas):
    canvas.configure(scrollregion=(0, 0, 1000, 1000))
    canvas.xview("moveto", 0.25)
    assert canvas.canvasx(0) == pytest.approx(250.0)
    assert canvas.canvasx(53) == pytest.approx(303.0)


def test_canvasy_maps_through_view_origin(canvas):
    canvas.configure(scrollregion=(0, 0, 1000, 1000))
    canvas.yview("moveto", 0.5)
    assert canvas.canvasy(0) == pytest.approx(500.0)
    assert canvas.canvasy(53) == pytest.approx(553.0)


def test_canvasx_gridspacing_snaps(canvas):
    canvas.configure(scrollregion=(0, 0, 1000, 1000))
    canvas.xview("moveto", 0.25)
    # 303 -> nearest multiple of 100 (halves away from zero: 450 -> 500).
    assert canvas.canvasx(53, 100) == pytest.approx(300.0)
    assert canvas.canvasx(200, 100) == pytest.approx(500.0)
    # gridspacing <= 0 disables the snap.
    assert canvas.canvasx(53, 0) == pytest.approx(303.0)
    assert canvas.canvasx(53, -10) == pytest.approx(303.0)


def test_canvasx_gridspacing_bad_number(canvas):
    canvas.configure(scrollregion=(0, 0, 1000, 1000))
    with pytest.raises(CommandError):
        canvas.canvasx(0, "wide")


def test_canvasxy_without_scrollregion(canvas):
    assert canvas.canvasx(12) == pytest.approx(12.0)
    assert canvas.canvasy(34) == pytest.approx(34.0)


# --- type -----------------------------------------------------------------------
def test_type_reports_first_match(canvas):
    rect = canvas.create_rectangle(0, 0, 10, 10, tags="g")
    line = canvas.create_line(0, 0, 10, 10, tags="g")
    assert canvas.type(rect) == "rectangle"
    assert canvas.type(line) == "line"
    # "g" matches both; the first (lowest) in the display list wins.
    assert canvas.type("g") == "rectangle"


def test_type_unknown_returns_none(canvas):
    assert canvas.type(999999) is None
    assert canvas.type("no-such-tag") is None


# --- focus ------------------------------------------------------------------------
def test_focus_empty_query_and_bogus_set(canvas):
    # No item supports the insertion cursor yet (only text items do), so
    # setting the focus on a rectangle is silently refused, like Tk.
    rect = canvas.create_rectangle(80, 80, 120, 120, fill="blue")
    assert canvas.focus() is None
    canvas.focus(rect)
    assert canvas.focus() is None
    canvas.focus(999999)  # no match: refused, not an error
    assert canvas.focus() is None
    canvas.focus("")  # reset is always allowed
    assert canvas.focus() is None


def test_focus_and_item_binding_clear_on_delete(canvas):
    rect = canvas.create_rectangle(80, 80, 120, 120, fill="blue")
    canvas.focus(rect)
    canvas.delete(rect)
    assert canvas.focus() is None


def test_focus_keyboard_events_do_not_crash(canvas):
    canvas.configure(takefocus=1)
    canvas.focus_force()
    canvas.create_rectangle(0, 0, 10, 10)
    canvas.master.update()
    canvas.event_generate("<KeyPress-a>", x=5, y=5)
    canvas.master.update()
