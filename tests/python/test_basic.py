"""Basic widget tests (native package: M2+, commands: M4+)."""

import pytest

tkinter = pytest.importorskip("tkinter")

from tkd2dcanvas.errors import CommandError  # noqa: E402


@pytest.fixture()
def root():
    try:
        r = tkinter.Tk()
    except tkinter.TclError as exc:
        pytest.skip(f"Tk unavailable in this environment: {exc}")
    yield r
    r.destroy()


@pytest.fixture()
def canvas(root):
    from tkd2dcanvas import Canvas

    try:
        return Canvas(root, width=320, height=240)
    except CommandError as exc:
        pytest.skip(f"native package not built yet (M2 pending): {exc}")


def _skip_unless_implemented(callable_):
    try:
        return callable_()
    except CommandError as exc:
        if "not implemented" in str(exc):
            pytest.skip(str(exc))
        raise


def test_canvas_creation(canvas):
    path = str(canvas)
    assert path.startswith(".")            # absolute Tk window path
    assert "tkd2dcanvas" in path


def test_create_rectangle(canvas):
    item = _skip_unless_implemented(lambda: canvas.create_rectangle(10, 10, 100, 80, fill="tomato"))
    assert isinstance(item, int) and item > 0


def test_bbox(canvas):
    item = _skip_unless_implemented(lambda: canvas.create_rectangle(10, 10, 100, 80))
    box = _skip_unless_implemented(lambda: canvas.bbox(item))
    assert box is not None
    assert box == pytest.approx((9, 9, 101, 81))


def test_bbox_is_integral_like_tkinter(canvas):
    """tkinter returns the canvas' bounding boxes as integers.

    Code that feeds a bbox into `wm geometry` (TinUI's tooltip and
    combobox do) relies on it: "279.0x51.0" is not a valid geometry.
    """
    item = _skip_unless_implemented(lambda: canvas.create_rectangle(10, 10, 100, 80))
    box = _skip_unless_implemented(lambda: canvas.bbox(item))
    assert box is not None
    assert all(isinstance(value, int) for value in box)
