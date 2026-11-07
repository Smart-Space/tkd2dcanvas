"""tkd2dcanvas — Direct2D Canvas for Tcl/Tk and Python tkinter.

Usage::

    import tkinter
    import tkd2dcanvas

    root = tkinter.Tk()
    tkd2dcanvas.load_into(root)
    canvas = tkd2dcanvas.Canvas(root, width=800, height=600)
    canvas.pack(fill="both", expand=True)
    canvas.create_rectangle(20, 20, 200, 120, fill="tomato")
"""

from ._loader import load_into
from .canvas import Canvas
from .errors import Tkd2dcanvasError, LoadError, CommandError
from . import events, options, items

__version__ = "0.1.0"

__all__ = [
    "Canvas",
    "load_into",
    "Tkd2dcanvasError",
    "LoadError",
    "CommandError",
    "events",
    "options",
    "items",
    "__version__",
]
