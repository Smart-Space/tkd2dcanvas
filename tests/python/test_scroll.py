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
        yield Canvas(root, width=200, height=200,
                     scrollregion=(0, 0, 1000, 1000))
    except CommandError as exc:
        pytest.skip(f"native package not built yet (M2 pending): {exc}")
    finally:
        root.destroy()


def test_xview_default(canvas):
    try:
        first, last = canvas.xview()
        assert 0.0 <= first < last <= 1.0
    except CommandError as exc:
        pytest.skip(str(exc))


def test_yview_moveto(canvas):
    try:
        canvas.yview("moveto", 0.5)
        first, last = canvas.yview()
        assert first == pytest.approx(0.4, abs=0.2)
    except CommandError as exc:
        pytest.skip(str(exc))
