"""Pythonic wrapper around the tkd2dcanvas Tcl widget."""

from __future__ import annotations

import itertools
import tkinter
from typing import Any, Iterable, Optional, Sequence, Tuple

from ._loader import load_into
from .errors import CommandError
from .options import flatten_options, serialize_value

_counter = itertools.count(1)


def _parse_id_list(text: str) -> list[int]:
    ids: list[int] = []
    for tok in text.split():
        try:
            ids.append(int(tok))
        except ValueError:
            pass
    return ids


def _parse_bbox(text: str) -> Optional[Tuple[float, float, float, float]]:
    parts = text.split()
    if len(parts) != 4:
        return None
    try:
        return (float(parts[0]), float(parts[1]), float(parts[2]), float(parts[3]))
    except ValueError:
        return None


class Canvas:
    """Direct2D canvas widget; API-compatible subset of tkinter.Canvas.

    Coordinates follow Tk conventions. ``create_*`` methods accept either
    ``x1, y1, x2, y2`` arguments or a flat ``*coords`` sequence.
    """

    def __init__(self, master: Optional[tkinter.Misc] = None, cnf: Optional[dict] = None, **kw: Any) -> None:
        if master is None:
            master = tkinter._get_default_root()  # type: ignore[attr-defined]
            if master is None:
                raise RuntimeError(
                    "no Tk root exists; create one with tkinter.Tk() first"
                )
        self.master = master
        load_into(master)

        options = dict(cnf or {})
        options.update(kw)
        args = ["tkd2dcanvas", self._new_path(master), *flatten_options(options)]
        try:
            self._path = str(master.tk.call(*args))
        except tkinter.TclError as exc:
            raise CommandError(f"failed to create tkd2dcanvas widget: {exc}") from exc

    # --- internals ----------------------------------------------------------
    @staticmethod
    def _new_path(master: tkinter.Misc) -> str:
        # The widget path must be absolute for Tk ("." + name, not ".." + name).
        name = f"tkd2dcanvas_{next(_counter)}"
        base = master._w
        return f".{name}" if base == "." else f"{base}.{name}"

    @property
    def tk_path(self) -> str:
        return self._path

    def __str__(self) -> str:  # tkinter compatibility: str(widget) == path
        return self._path

    def _call(self, *args: Any) -> str:
        try:
            return str(self.master.tk.call(self._path, *args))
        except tkinter.TclError as exc:
            raise CommandError(str(exc)) from exc

    # --- item creation -------------------------------------------------------
    def create(self, itemtype: str, *coords: Any, **kw: Any) -> int:
        args = [serialize_value(c) for c in coords]
        args += flatten_options(kw)
        return int(self._call("create", itemtype, *args))

    def create_rectangle(self, *args: Any, **kw: Any) -> int:
        return self.create("rectangle", *self._coords(args), **kw)

    def create_oval(self, *args: Any, **kw: Any) -> int:
        return self.create("oval", *self._coords(args), **kw)

    def create_line(self, *args: Any, **kw: Any) -> int:
        return self.create("line", *self._coords(args), **kw)

    def create_polygon(self, *args: Any, **kw: Any) -> int:
        return self.create("polygon", *self._coords(args), **kw)

    def create_arc(self, *args: Any, **kw: Any) -> int:
        return self.create("arc", *self._coords(args), **kw)

    def create_text(self, *args: Any, **kw: Any) -> int:
        return self.create("text", *self._coords(args), **kw)

    def create_image(self, *args: Any, **kw: Any) -> int:
        return self.create("image", *self._coords(args), **kw)

    @staticmethod
    def _coords(args: Sequence[Any]) -> list[Any]:
        # Accept both create_rectangle(x1, y1, x2, y2) and a single flat list.
        if len(args) == 1 and isinstance(args[0], (list, tuple)):
            return list(args[0])
        return list(args)

    # --- coordinates ----------------------------------------------------------
    def coords(self, item: Any, *args: Any):
        result = self._call("coords", serialize_value(item), *[serialize_value(a) for a in args])
        if args:
            return None
        return [float(v) for v in result.split()]

    def move(self, items: Any, dx: float, dy: float) -> None:
        self._call("move", serialize_value(items), serialize_value(dx), serialize_value(dy))

    def delete(self, *items: Any) -> None:
        self._call("delete", *[serialize_value(i) for i in items])

    # --- options ----------------------------------------------------------------
    def itemconfigure(self, item: Any, cnf: Optional[dict] = None, **kw: Any):
        if cnf:
            kw = {**cnf, **kw}
        if not kw:
            return self._call("itemconfigure", serialize_value(item))
        return self._call("itemconfigure", serialize_value(item), *flatten_options(kw))

    itemconfig = itemconfigure

    def itemcget(self, item: Any, option: str) -> str:
        return self._call("itemcget", serialize_value(item), f"-{option.lstrip('-')}")

    def configure(self, cnf: Optional[dict] = None, **kw: Any):
        if cnf:
            kw = {**cnf, **kw}
        if not kw:
            return self._call("configure")
        return self._call("configure", *flatten_options(kw))

    config = configure

    def cget(self, option: str) -> str:
        return self._call("cget", f"-{option.lstrip('-')}")

    # --- bbox / find / tags -----------------------------------------------------
    def bbox(self, *items: Any) -> Optional[Tuple[float, float, float, float]]:
        return _parse_bbox(self._call("bbox", *[serialize_value(i) for i in items]))

    def find_withtag(self, tag: Any) -> list[int]:
        return _parse_id_list(self._call("find", "withtag", serialize_value(tag)))

    def find_all(self) -> list[int]:
        return _parse_id_list(self._call("find", "all"))

    def find_closest(self, x: float, y: float, halo: Optional[float] = None) -> list[int]:
        args = [serialize_value(x), serialize_value(y)]
        if halo is not None:
            args.append(serialize_value(halo))
        return _parse_id_list(self._call("find", "closest", *args))

    def addtag_withtag(self, tag: Any, tagOrId: Any) -> None:
        self._call("addtag", serialize_value(tag), "withtag", serialize_value(tagOrId))

    def addtag_all(self, tag: Any) -> None:
        self._call("addtag", serialize_value(tag), "all")

    def dtag(self, item: Any, tag: Any) -> None:
        self._call("dtag", serialize_value(item), serialize_value(tag))

    def gettags(self, item: Any) -> list[str]:
        return self._call("gettags", serialize_value(item)).split()

    # --- scrolling --------------------------------------------------------------
    def xview(self, *args: Any):
        if not args:
            return tuple(float(v) for v in self._call("xview").split())
        return self._call("xview", *[serialize_value(a) for a in args])

    def yview(self, *args: Any):
        if not args:
            return tuple(float(v) for v in self._call("yview").split())
        return self._call("yview", *[serialize_value(a) for a in args])

    # --- geometry proxies ---------------------------------------------------------
    def pack(self, cnf: Optional[dict] = None, **kw: Any):
        return self._geometry_cmd("pack", cnf, kw)

    def grid(self, cnf: Optional[dict] = None, **kw: Any):
        return self._geometry_cmd("grid", cnf, kw)

    def place(self, cnf: Optional[dict] = None, **kw: Any):
        return self._geometry_cmd("place", cnf, kw)

    def _geometry_cmd(self, cmd: str, cnf: Optional[dict], kw: dict):
        if cnf:
            kw = {**cnf, **kw}
        return self.master.tk.call(cmd, self._path, *flatten_options(kw))

    # --- events ------------------------------------------------------------------
    def tag_bind(self, tag: Any, sequence: str, script: Optional[str] = None) -> None:
        args = [serialize_value(tag), sequence]
        if script is not None:
            args.append(script)
        self._call("bind", *args)

    bind = tag_bind

    # --- lifecycle -----------------------------------------------------------------
    def destroy(self) -> None:
        try:
            self.master.tk.call("destroy", self._path)
        except tkinter.TclError:
            pass  # already gone
