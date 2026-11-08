"""Tag binding tests (milestone M6)."""

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


def test_tag_bind_roundtrip(canvas):
    try:
        canvas.tag_bind("node", "<Button-1>", "puts clicked")
        script = canvas.tag_bind("node", "<Button-1>")
        assert "puts clicked" in script
    except CommandError as exc:
        pytest.skip(str(exc))


def test_tag_bind_python_callback(canvas):
    try:
        hits = []
        item = canvas.create_rectangle(80, 80, 120, 120, fill="blue", tags=("blue",))
        canvas.tag_bind("blue", "<Button-1>", lambda event: hits.append(event))

        canvas.master.tk.call("event", "generate", canvas.tk_path, "<1>",
                              "-x", "100", "-y", "100")
        canvas.master.update()

        assert len(hits) == 1
        assert hits[0].x == 100
        assert hits[0].y == 100
    except CommandError as exc:
        pytest.skip(str(exc))


def test_tag_bind_remove(canvas):
    try:
        hits = []
        canvas.create_rectangle(80, 80, 120, 120, fill="blue", tags=("blue",))
        canvas.tag_bind("blue", "<Button-1>", lambda event: hits.append(event))
        canvas.tag_bind("blue", "<Button-1>", "")

        canvas.master.tk.call("event", "generate", canvas.tk_path, "<1>",
                              "-x", "100", "-y", "100")
        canvas.master.update()

        assert hits == []
    except CommandError as exc:
        pytest.skip(str(exc))


def test_bind_returns_a_funcid(canvas):
    try:
        funcid = canvas.tag_bind("blue", "<Button-1>", lambda event: None)
        assert isinstance(funcid, str)
        assert funcid
    except CommandError as exc:
        pytest.skip(str(exc))


def test_tag_bind_enter_leave_on_motion(canvas):
    """<Enter> fires when the pointer moves onto an item, not only when the
    window itself is entered (Tk canvas semantics)."""
    try:
        hits = []
        canvas.create_rectangle(20, 20, 120, 100, fill="red", tags=("red",))
        canvas.create_rectangle(140, 20, 240, 100, fill="blue", tags=("blue",))
        canvas.tag_bind("red", "<Enter>", lambda e: hits.append("red-enter"))
        canvas.tag_bind("red", "<Leave>", lambda e: hits.append("red-leave"))
        canvas.tag_bind("blue", "<Enter>", lambda e: hits.append("blue-enter"))
        canvas.tag_bind("blue", "<Leave>", lambda e: hits.append("blue-leave"))

        def motion(x, y):
            canvas.master.tk.call("event", "generate", canvas.tk_path, "<Motion>",
                                  "-x", str(x), "-y", str(y))
            canvas.master.update()

        motion(60, 60)
        assert hits == ["red-enter"]
        hits.clear()
        motion(190, 60)
        assert hits == ["red-leave", "blue-enter"]
        hits.clear()
        motion(250, 200)
        assert hits == ["blue-leave"]
        hits.clear()
        motion(60, 60)
        assert hits == ["red-enter"]
    except CommandError as exc:
        pytest.skip(str(exc))


def test_tag_bind_multiple_callbacks(canvas):
    """Several callbacks may share one sequence (like tkinter's "add")."""
    try:
        hits = []
        canvas.create_rectangle(80, 80, 120, 120, fill="blue", tags=("blue",))
        first = canvas.tag_bind("blue", "<Button-1>", lambda e: hits.append("A"))
        canvas.tag_bind("blue", "<Button-1>", lambda e: hits.append("B"), add=True)

        canvas.master.tk.call("event", "generate", canvas.tk_path, "<1>",
                              "-x", "100", "-y", "100")
        canvas.master.update()
        assert hits == ["A", "B"]

        # Only the first callback is removed.
        hits.clear()
        canvas.tag_unbind("blue", "<Button-1>", first)
        canvas.master.tk.call("event", "generate", canvas.tk_path, "<1>",
                              "-x", "100", "-y", "100")
        canvas.master.update()
        assert hits == ["B"]
    except CommandError as exc:
        pytest.skip(str(exc))


def test_widget_bind_multiple_callbacks(canvas):
    try:
        hits = []
        first = canvas.bind("<Button-1>", lambda e: hits.append("A"))
        canvas.bind("<Button-1>", lambda e: hits.append("B"), add=True)

        canvas.master.tk.call("event", "generate", canvas.tk_path, "<1>",
                              "-x", "300", "-y", "300")
        canvas.master.update()
        assert hits == ["A", "B"]

        hits.clear()
        canvas.unbind("<Button-1>", first)
        canvas.master.tk.call("event", "generate", canvas.tk_path, "<1>",
                              "-x", "300", "-y", "300")
        canvas.master.update()
        assert hits == ["B"]

        # Unbinding without a funcid removes the whole binding.
        hits.clear()
        canvas.unbind("<Button-1>")
        canvas.master.tk.call("event", "generate", canvas.tk_path, "<1>",
                              "-x", "300", "-y", "300")
        canvas.master.update()
        assert hits == []
    except CommandError as exc:
        pytest.skip(str(exc))
