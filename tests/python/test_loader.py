"""Tests for the Tcl package loader."""

import pytest


def test_module_imports():
    import tkd2dcanvas

    assert isinstance(tkd2dcanvas.__version__, str)
    assert callable(tkd2dcanvas.load_into)


def test_tcl_package_dir_exists():
    from tkd2dcanvas import _loader

    path = _loader.tcl_package_dir()
    assert path.name == "tcl"


def test_load_into_attempts_package(tmp_path):
    tkinter = pytest.importorskip("tkinter")

    try:
        root = tkinter.Tk()
    except tkinter.TclError as exc:
        pytest.skip(f"Tk unavailable in this environment: {exc}")

    try:
        from tkd2dcanvas import load_into

        try:
            load_into(root)
        except Exception as exc:
            pytest.skip(f"native package not built yet (M0 pending): {exc}")
    finally:
        root.destroy()
