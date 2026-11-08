"""Item -state / -disabledfill (M5, Tk canvas item options)."""

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


def test_state_defaults_to_normal(canvas):
    item = canvas.create_rectangle(0, 0, 10, 10)
    assert canvas.itemcget(item, "state") == "normal"


def test_state_roundtrip_and_prefix(canvas):
    item = canvas.create_rectangle(0, 0, 10, 10)
    canvas.itemconfigure(item, state="disabled")
    assert canvas.itemcget(item, "state") == "disabled"
    # Tk accepts unique prefixes ("dis" -> disabled)
    canvas.itemconfigure(item, state="dis")
    assert canvas.itemcget(item, "state") == "disabled"
    canvas.itemconfigure(item, state="hidden")
    assert canvas.itemcget(item, "state") == "hidden"
    canvas.itemconfigure(item, state="normal")
    assert canvas.itemcget(item, "state") == "normal"


def test_state_rejects_bad_values(canvas):
    item = canvas.create_rectangle(0, 0, 10, 10)
    with pytest.raises(CommandError):
        canvas.itemconfigure(item, state="bogus")
    with pytest.raises(CommandError):
        canvas.itemconfigure(item, state="active")


def test_hidden_item_has_no_bbox_and_no_overlaps(canvas):
    item = canvas.create_rectangle(40, 40, 120, 100, fill="red", tags=("grp",))
    canvas.itemconfigure(item, state="hidden")
    assert canvas.bbox(item) is None
    overlapping = canvas.find_overlapping(0, 0, 320, 240)
    assert item not in overlapping
    assert item in canvas.find_withtag("grp")
    assert item in canvas.find_all()


def test_disabled_item_is_not_pickable(canvas):
    item = canvas.create_rectangle(40, 40, 120, 100, fill="red", tags=("grp",))
    hits = []
    canvas.tag_bind("grp", "<Enter>", lambda e: hits.append("enter"))
    canvas.itemconfigure(item, state="disabled")
    canvas.master.tk.call("event", "generate", canvas.tk_path, "<Motion>",
                          "-x", "60", "-y", "60")
    canvas.master.update()
    assert hits == []
    # Tk keeps disabled items in "find closest"
    assert canvas.find_closest(60, 60) == (item,)
    assert item in canvas.find_overlapping(0, 0, 320, 240)


def test_enabled_item_is_pickable(canvas):
    item = canvas.create_rectangle(40, 40, 120, 100, fill="red", tags=("grp",))
    hits = []
    canvas.tag_bind("grp", "<Enter>", lambda e: hits.append("enter"))
    canvas.master.tk.call("event", "generate", canvas.tk_path, "<Motion>",
                          "-x", "60", "-y", "60")
    canvas.master.update()
    assert hits == ["enter"]


def test_disabledfill_roundtrip(canvas):
    item = canvas.create_oval(40, 40, 120, 100, fill="tomato", disabledfill="#888888")
    assert canvas.itemcget(item, "disabledfill") == "#888888"
    canvas.itemconfigure(item, disabledfill="#123123")
    assert canvas.itemcget(item, "disabledfill") == "#123123"
    # Empty string clears it (Tk semantics) and falls back to -fill
    canvas.itemconfigure(item, disabledfill="")
    assert canvas.itemcget(item, "disabledfill") == ""


def test_disabledfill_rejected_on_image_items(canvas):
    item = canvas.create_image(40, 40)
    with pytest.raises(CommandError):
        canvas.itemconfigure(item, disabledfill="#123123")


def test_itemconfigure_report_reports_new_options(canvas):
    item = canvas.create_rectangle(0, 0, 10, 10)
    report = canvas.itemconfigure(item)
    assert report["state"] == "normal"
    assert report["disabledfill"] == ""


def test_state_hidden_restores_drawing_bbox(canvas):
    item = canvas.create_rectangle(40, 40, 120, 100, fill="red")
    canvas.itemconfigure(item, state="hidden")
    assert canvas.bbox(item) is None
    canvas.itemconfigure(item, state="normal")
    assert canvas.bbox(item) is not None
