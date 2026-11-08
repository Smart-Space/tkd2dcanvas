"""Stage 7 (M8): Widget integration, aliases and loader boundaries.

These tests cover what ``test_basic``/``test_bind``/... do not: the
tkinter.Widget registry behaviour (nametowidget, winfo_children), the dict
report form of configure()/itemconfigure(), the Tk 4-style aliases, and the
loader edge cases (missing master, missing Tcl package contents).
"""

import pytest

tkinter = pytest.importorskip("tkinter")

from tkd2dcanvas.errors import CommandError, LoadError  # noqa: E402


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


# --- widget integration -------------------------------------------------------
def test_canvas_is_tkinter_canvas(canvas):
    assert isinstance(canvas, tkinter.Canvas)
    assert isinstance(canvas, tkinter.Widget)


def test_nametowidget_roundtrip(root, canvas):
    assert root.nametowidget(str(canvas)) is canvas


def test_winfo_children_lists_canvas(root, canvas):
    assert canvas in root.winfo_children()


def test_relative_name_and_master_registration(root, canvas):
    assert isinstance(canvas._name, str)
    assert canvas._w == ("." + canvas._name) if canvas.master._w == "." else True
    assert root.children.get(canvas._name) is canvas
    # The full path must not leak into the relative child name.
    assert not canvas._name.startswith(".")


def test_two_canvases_get_distinct_paths(root):
    from tkd2dcanvas import Canvas

    a = Canvas(root, width=10, height=10)
    b = Canvas(root, width=10, height=10)
    try:
        assert a._w != b._w
        assert a._name != b._name
        assert root.children.get(a._name) is a
        assert root.children.get(b._name) is b
    finally:
        a.destroy()
        b.destroy()


def test_keys_matches_dict_report(canvas):
    keys = canvas.keys()
    assert isinstance(keys, list)
    assert "width" in keys
    report = canvas.configure()
    assert isinstance(report, dict)
    assert list(report) == keys


def test_configure_report_is_dict(canvas):
    report = canvas.configure()
    assert isinstance(report, dict)
    assert report["width"] == 320
    assert report["height"] == 240


def test_configure_set_returns_none(canvas):
    assert canvas.configure(width=200) is None
    assert canvas.cget("width") == "200"
    # And the config = configure alias behaves the same.
    assert canvas.config(height=100) is None
    assert canvas.cget("height") == "100"


def test_configure_single_option_query(canvas):
    assert canvas.configure("width") == 320


def test_itemconfigure_report_is_dict(canvas):
    item = canvas.create_rectangle(0, 0, 10, 10, fill="#ff0000")
    report = canvas.itemconfigure(item)
    assert isinstance(report, dict)
    assert report["fill"] == "#ff0000"


def test_itemconfigure_set_returns_none(canvas):
    item = canvas.create_rectangle(0, 0, 10, 10)
    assert canvas.itemconfigure(item, fill="#008000") is None
    assert canvas.itemcget(item, "fill") == "#008000"
    assert canvas.itemconfigure(item, "width") == "1"


def test_itemconfigure_no_match_returns_empty_dict(canvas):
    assert canvas.itemconfigure(999999) == {}


def test_pack_grid_place_return_none(canvas):
    assert canvas.pack() is None
    assert canvas.grid() is None
    assert canvas.place() is None


def test_destroy_unregisters_from_master(root, canvas):
    name = canvas._name
    path = str(canvas)
    canvas.destroy()
    assert name not in root.children
    assert canvas not in root.winfo_children()
    with pytest.raises(KeyError):
        root.nametowidget(path)


def test_destroy_twice_is_safe(canvas):
    canvas.destroy()
    canvas.destroy()  # must not raise


# --- find / gettags return tuples ---------------------------------------------
def test_find_results_are_tuples(canvas):
    item = canvas.create_rectangle(0, 0, 10, 10)
    assert isinstance(canvas.find_all(), tuple)
    assert isinstance(canvas.find_withtag("all"), tuple)
    assert isinstance(canvas.find_closest(5, 5), tuple)
    assert isinstance(canvas.find_overlapping(0, 0, 100, 100), tuple)
    assert isinstance(canvas.find_enclosed(0, 0, 100, 100), tuple)
    assert item in canvas.find_all()


def test_gettags_returns_tuple(canvas):
    item = canvas.create_rectangle(0, 0, 10, 10, tags=("alpha", "beta"))
    assert canvas.gettags(item) == ("alpha", "beta")
    assert isinstance(canvas.gettags(item), tuple)


# --- Tk 4-style aliases ---------------------------------------------------------
def test_addtag_above_and_below(canvas):
    a = canvas.create_rectangle(0, 0, 10, 10)
    b = canvas.create_rectangle(20, 20, 30, 30)
    canvas.addtag_above("hot", a)   # everything above a
    assert canvas.find_withtag("hot") == (b,)
    canvas.dtag("hot", "hot")
    canvas.addtag_below("cold", b)  # everything below b
    assert canvas.find_withtag("cold") == (a,)


def test_addtag_closest(canvas):
    a = canvas.create_rectangle(0, 0, 10, 10)
    canvas.addtag_closest("near", 5, 5)
    assert canvas.find_withtag("near") == (a,)


def test_addtag_closest_with_halo(canvas):
    a = canvas.create_rectangle(0, 0, 10, 10)
    canvas.addtag_closest("near", 500, 500, halo=1000)
    assert canvas.find_withtag("near") == (a,)


def test_find_closest_start_argument(canvas):
    a = canvas.create_rectangle(0, 0, 10, 10)
    b = canvas.create_rectangle(100, 100, 110, 110)
    # start below a match makes the search wrap over everything else.
    found = canvas.find_closest(5, 5, start=b)
    assert found == (a,)


def test_tk4_aliases(canvas):
    item = canvas.create_rectangle(0, 0, 10, 10, tags="grp")
    assert canvas.findwithtag("grp") == (item,)
    assert canvas.findall() == (item,)
    assert canvas.findclosest(5, 5) == (item,)
    assert canvas.tagbind is not None
    assert callable(canvas.tagunbind)
    assert callable(canvas.itemconfig)
    assert callable(canvas.config)


def test_unimplemented_surface_raises_not_implemented():
    # These are valid tkinter.Canvas names, but the native package has no
    # implementation for them; they must raise a clear Python error.
    # (create_text / create_image / create_bitmap / create_window are
    # implemented since M9/M10.)
    from tkd2dcanvas import Canvas

    assert callable(Canvas.create_window)
    assert callable(Canvas.postscript)
    assert callable(Canvas.create_text)
    assert callable(Canvas.select_item)


def test_unimplemented_surface_calls_raise(canvas):
    with pytest.raises(NotImplementedError):
        canvas.postscript()
    with pytest.raises(NotImplementedError):
        canvas.select_item()


# --- loader boundaries -----------------------------------------------------------
def test_load_into_requires_a_master():
    from tkd2dcanvas import load_into

    with pytest.raises(LoadError):
        load_into(None)


def test_load_into_rejects_master_without_tk():
    from tkd2dcanvas import load_into

    class FakeMaster:
        pass

    with pytest.raises(LoadError):
        load_into(FakeMaster())


def test_load_into_missing_pkg_index(tmp_path, monkeypatch):
    import tkinter

    from tkd2dcanvas import _loader

    try:
        root = tkinter.Tk()
    except tkinter.TclError as exc:
        pytest.skip(f"Tk unavailable in this environment: {exc}")
    try:
        empty = tmp_path / "tcl"
        empty.mkdir()
        monkeypatch.setattr(_loader, "tcl_package_dir", lambda: empty)
        with pytest.raises(LoadError, match="pkgIndex.tcl"):
            _loader.load_into(root)
    finally:
        root.destroy()


def test_load_into_is_idempotent(root):
    from tkd2dcanvas import load_into
    from tkd2dcanvas._loader import is_loaded

    load_into(root)
    assert is_loaded(root)
    load_into(root)  # second call must be a no-op, not an error
    assert is_loaded(root)
