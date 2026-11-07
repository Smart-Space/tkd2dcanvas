"""M5: itemconfigure / itemconfig / itemcget."""

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


def test_itemconfigure_roundtrip(canvas):
    item = canvas.create_rectangle(0, 0, 10, 10)
    canvas.itemconfigure(item, fill="#ff0000")
    assert canvas.itemcget(item, "fill") == "#ff0000"


def test_itemcget_defaults(canvas):
    item = canvas.create_rectangle(0, 0, 10, 10)
    assert canvas.itemcget(item, "width") == "1"
    assert canvas.itemcget(item, "outline") == "#000000"
    assert canvas.itemcget(item, "fill") == ""
    assert canvas.itemcget(item, "dash") == ""


def test_itemconfigure_dash(canvas):
    item = canvas.create_line(0, 0, 10, 10)
    canvas.itemconfigure(item, dash="8 4")
    assert canvas.itemcget(item, "dash") == "8 4"


def test_itemconfigure_empty_fill(canvas):
    item = canvas.create_rectangle(0, 0, 10, 10, fill="red")
    canvas.itemconfigure(item, fill="")
    assert canvas.itemcget(item, "fill") == ""


def test_itemconfig_alias(canvas):
    item = canvas.create_oval(0, 0, 10, 10)
    canvas.itemconfig(item, width=4)
    assert canvas.itemcget(item, "width") == "4"


def test_itemconfigure_report(canvas):
    item = canvas.create_rectangle(0, 0, 10, 10, fill="#ff0000")
    report = canvas.itemconfigure(item)
    assert "fill" in report
    assert "#ff0000" in report


def test_itemconfigure_atomic(canvas):
    item = canvas.create_rectangle(0, 0, 10, 10)
    canvas.itemconfigure(item, width=2)
    with pytest.raises(CommandError):
        canvas.itemconfigure(item, width=9, bogus="1")
    assert canvas.itemcget(item, "width") == "2"


def test_itemconfigure_multiple_by_tag(canvas):
    a = canvas.create_rectangle(0, 0, 10, 10, tags="grp")
    b = canvas.create_oval(20, 20, 30, 30, tags="grp")
    canvas.itemconfigure("grp", width=5)
    assert canvas.itemcget(a, "width") == "5"
    assert canvas.itemcget(b, "width") == "5"
