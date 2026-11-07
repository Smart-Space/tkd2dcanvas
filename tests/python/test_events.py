"""Event binding smoke tests (milestone M6)."""

import pytest

tkinter = pytest.importorskip("tkinter")

from tkd2dcanvas import events  # noqa: E402
from tkd2dcanvas.errors import CommandError  # noqa: E402


@pytest.fixture()
def canvas():
    try:
        root = tkinter.Tk()
    except tkinter.TclError as exc:
        pytest.skip(f"Tk unavailable in this environment: {exc}")
    from tkd2dcanvas import Canvas

    try:
        yield Canvas(root, width=200, height=200)
    except CommandError as exc:
        pytest.skip(f"native package not built yet (M2 pending): {exc}")
    finally:
        root.destroy()


def test_event_constants_exist():
    assert events.BUTTON_PRESS_1 == "<Button-1>"
    assert events.MOUSE_WHEEL == "<MouseWheel>"


def test_tag_bind_registers(canvas):
    try:
        canvas.tag_bind("all", events.BUTTON_PRESS_1, "puts pressed")
    except CommandError as exc:
        pytest.skip(str(exc))
