"""Locate and load the bundled Tcl package into a Tk interpreter."""

from __future__ import annotations

from pathlib import Path
from typing import Optional

import tkinter

from .errors import LoadError

_PKG_DIR = Path(__file__).resolve().parent
_TCL_PKG_DIR = _PKG_DIR / "tcl"          # site-packages/tkd2dcanvas/tcl
_REPO_TCL_DIR = _PKG_DIR.parent.parent / "tcl"  # dev checkout fallback


def _candidate_tcl_dirs() -> list[Path]:
    dirs: list[Path] = []
    seen: set[Path] = set()
    for d in (_TCL_PKG_DIR, _REPO_TCL_DIR):
        try:
            rd = d.resolve()
        except OSError:
            rd = d
        if rd in seen:
            continue
        seen.add(rd)
        dirs.append(d)
    return dirs


def tcl_package_dir() -> Path:
    """Directory that contains the tkd2dcanvas Tcl package tree."""
    for d in _candidate_tcl_dirs():
        if (d / "tkd2dcanvas" / "pkgIndex.tcl").is_file():
            return d
    for d in _candidate_tcl_dirs():
        if d.is_dir():
            return d
    return _TCL_PKG_DIR


def load_into(master: Optional[tkinter.Misc] = None) -> None:
    if master is None:
        raise LoadError(
            "load_into() requires a Tk root or widget; "
            "create one with tkinter.Tk() first"
        )

    try:
        tk = master.tk
    except AttributeError as exc:
        raise LoadError(f"master {master!r} has no Tk interpreter: {exc}") from exc

    try:
        present = tk.call("package", "present", "tkd2dcanvas")
    except tkinter.TclError:
        present = ""
    if present:
        return

    tcl_dir = tcl_package_dir()
    pkg_dir = tcl_dir / "tkd2dcanvas"

    if not (pkg_dir / "pkgIndex.tcl").is_file():
        raise LoadError(
            "tkd2dcanvas Tcl package not found under "
            f"{tcl_dir}: missing {pkg_dir / 'pkgIndex.tcl'}. "
            "Did you build the native module? See docs/building.md."
        )

    posix_dir = tcl_dir.as_posix()
    try:
        tk.call("lappend", "auto_path", posix_dir)
        tk.call("package", "require", "tkd2dcanvas")
    except (tkinter.TclError, RuntimeError) as exc:
        raise LoadError(
            "failed to load the tkd2dcanvas Tcl package from "
            f"{pkg_dir}: {exc}. "
            "Did you build the native module? See docs/building.md."
        ) from exc


def is_loaded(master: tkinter.Misc) -> bool:
    try:
        tk = master.tk
    except AttributeError:
        return False
    try:
        return bool(tk.call("package", "present", "tkd2dcanvas"))
    except tkinter.TclError:
        return False
