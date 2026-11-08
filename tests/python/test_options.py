"""Option serialization boundaries (milestone M8).

The Tcl list quoting in ``_brace`` is exercised end-to-end through a real
Tcl interpreter — a silent misquote here would corrupt option values like
``-tags`` in ways that unit tests of the string alone would not catch.
"""

import pytest

tkinter = pytest.importorskip("tkinter")

from tkd2dcanvas.options import (  # noqa: E402
    _brace,
    flatten_options,
    serialize_value,
    tk_option,
)


@pytest.fixture()
def root():
    try:
        r = tkinter.Tk()
    except tkinter.TclError as exc:
        pytest.skip(f"Tk unavailable in this environment: {exc}")
    yield r
    r.destroy()


def _roundtrip(root, s):
    """Serialize ``s`` as one list element and parse it back with Tcl."""
    return root.tk.eval(f"return {_brace(s)}")


def _list_roundtrip(root, elements):
    """Serialize a list of elements and parse it back with Tcl."""
    joined = serialize_value(elements)
    return root.tk.splitlist(root.tk.eval(f"return [list {joined}]"))


@pytest.mark.parametrize("text", [
    "", "a", "a b", "a{b", "a}b", "a{b}c", "{}{}", "with \\ backslash",
    "line\nbreak", "100%", "-fill", "o'brien", 'q"q',
])
def test_brace_roundtrip(root, text):
    assert _roundtrip(root, text) == text


@pytest.mark.parametrize("text", [
    "", "a{b", "a}b", "a{b}c", "with \\ backslash", "line\nbreak", 'q"q',
])
def test_brace_list_roundtrip(root, text):
    """The element must survive a full list serialization too."""
    items = _list_roundtrip(root, [text])
    assert len(items) == 1
    assert items[0] == text


def test_serialize_list_with_unbalanced_braces(root):
    items = _list_roundtrip(root, ["a{b", "x y", "a}b"])
    assert list(items) == ["a{b", "x y", "a}b"]


def test_serialize_bytes():
    assert serialize_value(b"red") == "red"


def test_serialize_invalid_utf8_bytes():
    with pytest.raises(ValueError):
        serialize_value(b"\xff\xfe")


def test_serialize_bool_and_none():
    assert serialize_value(True) == "1"
    assert serialize_value(False) == "0"
    assert serialize_value(None) == ""


def test_serialize_set_is_deterministic():
    assert serialize_value({"b", "a"}) == "a b"


def test_flatten_options_none_and_options():
    assert flatten_options(None) == []
    assert flatten_options({"fill": "red", "dash": "8 4", "none_": None}) == [
        "-fill", "red", "-dash", "8 4",
    ]


def test_tk_option_idempotent_on_dashed_names():
    assert tk_option("fill") == "-fill"
    assert tk_option("-fill") == "-fill"
    assert tk_option("x_scroll") == "-x-scroll"
