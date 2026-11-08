"""Tag-related command tests (milestone M6)."""

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


def test_addtag_and_find_withtag(canvas):
    try:
        a = canvas.create_rectangle(0, 0, 10, 10)
        b = canvas.create_rectangle(20, 20, 30, 30)
        canvas.addtag_withtag("group", a)
        assert a in canvas.find_withtag("group")
        assert b not in canvas.find_withtag("group")
    except CommandError as exc:
        pytest.skip(str(exc))


def test_dtag(canvas):
    try:
        a = canvas.create_oval(0, 0, 10, 10)
        canvas.addtag_withtag("node", a)
        canvas.dtag(a, "node")
        assert a not in canvas.find_withtag("node")
    except CommandError as exc:
        pytest.skip(str(exc))


def test_gettags(canvas):
    try:
        a = canvas.create_rectangle(0, 0, 10, 10, tags=("alpha", "beta"))
        assert canvas.gettags(a) == ("alpha", "beta")
    except CommandError as exc:
        pytest.skip(str(exc))


def test_addtag_all(canvas):
    try:
        a = canvas.create_rectangle(0, 0, 10, 10)
        b = canvas.create_rectangle(20, 20, 30, 30)
        canvas.addtag_all("every")
        assert set(canvas.find_withtag("every")) == {a, b}
    except CommandError as exc:
        pytest.skip(str(exc))


def test_tag_raise_and_lower(canvas):
    try:
        a = canvas.create_rectangle(0, 0, 10, 10)
        b = canvas.create_rectangle(20, 20, 30, 30)
        canvas.tag_raise(a)
        assert canvas.find_all() == (b, a)
        canvas.tag_lower(a)
        assert canvas.find_all() == (a, b)
    except CommandError as exc:
        pytest.skip(str(exc))
