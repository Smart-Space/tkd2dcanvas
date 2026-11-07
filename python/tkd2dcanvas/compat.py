"""tkinter.Canvas compatibility layer.

mkdir: the goal of this module is a drop-in replacement for the common
tkinter.Canvas API surface. See docs/tk_canvas_compat_matrix.md for what is
supported at this release.
"""

from __future__ import annotations

from .canvas import Canvas as Tkd2dcanvas
from ._loader import load_into


class Canvas(Tkd2dcanvas):
    """Drop-in style Canvas with a tkinter.Canvas-like construction signature."""


def install(root) -> None:
    """Explicitly load the Tcl package into ``root`` (usually not needed —
    creating a Canvas does it automatically)."""
    load_into(root)


__all__ = ["Canvas", "install"]
