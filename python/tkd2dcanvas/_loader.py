"""Locate and load the bundled Tcl package into a Tk interpreter."""

from pathlib import Path
import sys
import tkinter

from .errors import LoadError

_PKG_DIR = Path(__file__).resolve().parent
_TCL_PKG_DIR = _PKG_DIR / "tcl"          # site-packages/tkd2dcanvas/tcl
_REPO_TCL_DIR = _PKG_DIR.parent.parent / "tcl"  # dev checkout fallback

_loaded: set[int] = set()


def _candidate_tcl_dirs() -> list[Path]:
    dirs = [_TCL_PKG_DIR]
    if _REPO_TCL_DIR.is_dir():
        dirs.append(_REPO_TCL_DIR)
    return dirs


def tcl_package_dir() -> Path:
    """Directory that contains the tkd2dcanvas Tcl package tree."""
    for d in _candidate_tcl_dirs():
        if (d / "tkd2dcanvas" / "pkgIndex.tcl").is_file():
            return d
    return _TCL_PKG_DIR


def load_into(master: tkinter.Misc) -> None:
    """Add the bundled Tcl package to the interpreter and require it.

    Idempotent per interpreter; safe to call before every widget creation.
    """
    tk = master.tk
    key = id(tk)
    if key in _loaded:
        return

    tcl_dir = tcl_package_dir().as_posix()
    pkg_dir = tcl_dir + "/tkd2dcanvas"

    try:
        tk.eval(f"lappend auto_path {{{tcl_dir}}}")
        version = tk.call("package", "require", "tkd2dcanvas")
    except tkinter.TclError as exc:
        raise LoadError(
            "failed to load the tkd2dcanvas Tcl package from "
            f"{pkg_dir}: {exc}. "
            "Did you build the native module? See docs/building.md."
        ) from exc

    _ = version  # e.g. "0.1"
    _loaded.add(key)


def is_loaded(master: tkinter.Misc) -> bool:
    return id(master.tk) in _loaded
