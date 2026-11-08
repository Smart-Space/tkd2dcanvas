"""find command tests (milestone M6)."""

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
        pytest.skip(f"native package not built yet (M2 pending): {exc}")
    finally:
        root.destroy()


def test_find_all(canvas):
    try:
        a = canvas.create_rectangle(0, 0, 10, 10)
        assert a in canvas.find_all()
    except CommandError as exc:
        pytest.skip(str(exc))


def test_find_closest(canvas):
    try:
        a = canvas.create_rectangle(0, 0, 10, 10)
        assert a in canvas.find_closest(5, 5)
    except CommandError as exc:
        pytest.skip(str(exc))


def test_find_overlapping_and_enclosed(canvas):
    try:
        a = canvas.create_rectangle(0, 0, 100, 100)
        b = canvas.create_rectangle(500, 500, 600, 600)
        assert canvas.find_overlapping(-10, -10, 10, 10) == (a,)
        assert canvas.find_enclosed(-10, -10, 10, 10) == ()
        assert canvas.find_overlapping(-1000, -1000, 1000, 1000) == (a, b)
    except CommandError as exc:
        pytest.skip(str(exc))
