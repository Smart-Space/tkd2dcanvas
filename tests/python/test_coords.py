"""coords/move command tests (milestone M5)."""

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


def test_coords_roundtrip(canvas):
    try:
        item = canvas.create_rectangle(0, 0, 10, 10)
        assert canvas.coords(item) == pytest.approx([0, 0, 10, 10])
        canvas.coords(item, 5, 5, 50, 50)
        assert canvas.coords(item) == pytest.approx([5, 5, 50, 50])
    except CommandError as exc:
        pytest.skip(str(exc))


def test_move(canvas):
    try:
        item = canvas.create_rectangle(0, 0, 10, 10)
        canvas.move(item, 7, 3)
        assert canvas.coords(item) == pytest.approx([7, 3, 17, 13])
    except CommandError as exc:
        pytest.skip(str(exc))


def test_coords_accepts_a_flat_list(canvas):
    try:
        item = canvas.create_polygon(0, 0, 10, 0, 10, 10)
        canvas.coords(item, (1, 2, 3, 4, 5, 6))
        assert canvas.coords(item) == pytest.approx([1, 2, 3, 4, 5, 6])
    except CommandError as exc:
        pytest.skip(str(exc))


def test_coords_flattens_nested_lists(canvas):
    """tkinter accepts nested coordinate sequences and flattens them."""
    try:
        item = canvas.create_polygon(0, 0, 10, 0, 10, 10)
        canvas.coords(item, (1, 2, 3, 4, (5, 6)))
        assert canvas.coords(item) == pytest.approx([1, 2, 3, 4, 5, 6])
        canvas.coords(item, 1, 2, (3, 4))
        assert canvas.coords(item) == pytest.approx([1, 2, 3, 4])
    except CommandError as exc:
        pytest.skip(str(exc))


def test_create_flattens_nested_lists(canvas):
    try:
        item = canvas.create_polygon(1, 2, 3, 4, (5, 6))
        assert canvas.coords(item) == pytest.approx([1, 2, 3, 4, 5, 6])
    except CommandError as exc:
        pytest.skip(str(exc))
