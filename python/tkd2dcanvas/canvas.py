"""Pythonic wrapper around the tkd2dcanvas Tcl widget."""

from __future__ import annotations

import itertools
import tkinter
from typing import Any, Callable, Optional, Sequence, Tuple

from ._loader import load_into
from .errors import CommandError
from .options import flatten_options, serialize_value

_counter = itertools.count(1)
_command_counter = itertools.count(1)

# The substitutions Tk performs on a binding script before it is evaluated.
# The order matches tkinter's own widget bindings, so that a callback can be
# written exactly like in tkinter.
_SUBST_FORMAT = (
    "%#", "%b", "%f", "%h", "%k", "%s", "%t", "%w", "%x", "%y",
    "%A", "%E", "%K", "%N", "%W", "%T", "%X", "%Y", "%D",
)
_SUBST_FORMAT_STR = " ".join(_SUBST_FORMAT)


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


def _make_event(canvas: "Canvas", args: Sequence[str]) -> Any:
    """Turns the substituted binding arguments into a tkinter.Event."""
    if len(args) != len(_SUBST_FORMAT):
        return args

    def as_int(value: str) -> Any:
        try:
            return int(value)
        except ValueError:
            return value

    (nsign, b, f, h, k, s, t, w, x, y, a, e, K, N, W, T, X, Y, D) = args

    event = tkinter.Event()
    event.serial = as_int(nsign)
    event.num = as_int(b)
    try:
        event.focus = bool(canvas.master.tk.getboolean(f))
    except tkinter.TclError:
        pass
    event.height = as_int(h)
    event.keycode = as_int(k)
    event.state = as_int(s)
    event.time = as_int(t)
    event.width = as_int(w)
    event.x = as_int(x)
    event.y = as_int(y)
    event.char = a
    try:
        event.send_event = bool(canvas.master.tk.getboolean(e))
    except tkinter.TclError:
        pass
    event.keysym = K
    event.keysym_num = as_int(N)
    event.type = T
    event.widget = W
    event.x_root = as_int(X)
    event.y_root = as_int(Y)
    try:
        event.delta = int(D)
    except ValueError:
        event.delta = 0
    return event


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

        # Tcl commands registered for Python callbacks (see _register).
        self._tcl_commands: list[str] = []

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
            result = self.master.tk.call(self._path, *args)
        except tkinter.TclError as exc:
            raise CommandError(str(exc)) from exc
        if isinstance(result, tuple):
            # A Tcl list comes back as a tuple of strings.
            return " ".join(str(part) for part in result)
        return str(result)

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

    def find_overlapping(self, x1: float, y1: float, x2: float, y2: float) -> list[int]:
        return _parse_id_list(
            self._call("find", "overlapping", serialize_value(x1), serialize_value(y1),
                       serialize_value(x2), serialize_value(y2))
        )

    def find_enclosed(self, x1: float, y1: float, x2: float, y2: float) -> list[int]:
        return _parse_id_list(
            self._call("find", "enclosed", serialize_value(x1), serialize_value(y1),
                       serialize_value(x2), serialize_value(y2))
        )

    def addtag_withtag(self, tag: Any, tagOrId: Any) -> None:
        self._call("addtag", serialize_value(tag), "withtag", serialize_value(tagOrId))

    def addtag_all(self, tag: Any) -> None:
        self._call("addtag", serialize_value(tag), "all")

    def addtag_overlapping(self, tag: Any, x1: float, y1: float, x2: float, y2: float) -> None:
        self._call("addtag", serialize_value(tag), "overlapping", serialize_value(x1),
                   serialize_value(y1), serialize_value(x2), serialize_value(y2))

    def addtag_enclosed(self, tag: Any, x1: float, y1: float, x2: float, y2: float) -> None:
        self._call("addtag", serialize_value(tag), "enclosed", serialize_value(x1),
                   serialize_value(y1), serialize_value(x2), serialize_value(y2))

    def dtag(self, item: Any, tag: Any) -> None:
        self._call("dtag", serialize_value(item), serialize_value(tag))

    def gettags(self, item: Any) -> list[str]:
        return self._call("gettags", serialize_value(item)).split()

    def tag_raise(self, item: Any, aboveThis: Any = None) -> None:
        args = [serialize_value(item)]
        if aboveThis is not None:
            args.append(serialize_value(aboveThis))
        self._call("raise", *args)

    def tag_lower(self, item: Any, belowThis: Any = None) -> None:
        args = [serialize_value(item)]
        if belowThis is not None:
            args.append(serialize_value(belowThis))
        self._call("lower", *args)

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
    def tag_bind(self, tag: Any, sequence: Optional[str] = None,
                 func: Optional[Any] = None, add: Optional[bool] = None) -> Any:
        """Binds an event sequence to a tag (or to a single item).

        ``func`` may be a Python callable or a Tcl script.  A callable is
        registered as a Tcl command and receives the event object; the id of
        that command is returned so that the callback can be removed again
        with :meth:`tag_unbind`.  With ``add=True`` the callback is added to
        the ones that are already bound to the sequence, otherwise it
        replaces them.  Without ``func`` the bound script is returned.
        """
        return self._bind((self._path, "bind", serialize_value(tag)),
                           sequence, func, add)

    def bind(self, sequence: Optional[str] = None, func: Optional[Any] = None,
             add: Optional[bool] = None) -> Any:
        """Binds an event sequence to the canvas widget itself.

        Like tkinter, this is the *widget* binding; use :meth:`tag_bind` for
        the items of the canvas.  See :meth:`tag_bind` for the meaning of
        ``func`` and ``add``.
        """
        return self._bind(("bind", self._path), sequence, func, add)

    def tag_unbind(self, tag: Any, sequence: str, funcid: Optional[str] = None) -> None:
        """Removes the binding of ``sequence`` for a tag (or a single item).

        With ``funcid`` only that single callback is removed, otherwise the
        whole binding of the sequence goes away.
        """
        self._unbind((self._path, "bind", serialize_value(tag), sequence), funcid)

    def unbind(self, sequence: str, funcid: Optional[str] = None) -> None:
        """Removes the widget binding of ``sequence``.

        With ``funcid`` only that single callback is removed, otherwise the
        whole binding of the sequence goes away.
        """
        self._unbind(("bind", self._path, sequence), funcid)

    def _bind(self, what: tuple, sequence: Optional[str], func: Any,
              add: Optional[bool]) -> Any:
        """The tkinter binding protocol (see tkinter.Misc._bind)."""
        if isinstance(func, str):
            # A plain Tcl script replaces the whole binding.
            self.master.tk.call(what + (sequence, func))
            return None
        if func:
            funcid = self._register(func)
            script = ('%sif {"[%s %s]" == "break"} break\n'
                      % ("+" if add else "", funcid, _SUBST_FORMAT_STR))
            self.master.tk.call(what + (sequence, script))
            return funcid
        if sequence:
            return self.master.tk.call(what + (sequence,))
        return self.master.tk.splitlist(self.master.tk.call(what))

    def _unbind(self, what: tuple, funcid: Optional[str] = None) -> None:
        """The tkinter unbinding protocol (see tkinter.Misc._unbind).

        A single callback is identified by the line it contributed to the
        script, so that the other callbacks of the same sequence survive.
        """
        if funcid is None:
            self.master.tk.call(*what, "")
            return

        script = self.master.tk.call(what)
        prefix = 'if {"[%s ' % funcid
        keep = "\n".join(line for line in script.split("\n")
                         if not line.startswith(prefix))
        if not keep.strip():
            keep = ""
        self.master.tk.call(*what, keep)
        self._delete_command(funcid)

    def _register(self, func: Callable[..., Any]) -> str:
        """Registers a Python callable as a Tcl command and returns its name.

        Every registration creates its own command (like tkinter does), so
        that a single callback can be removed again without affecting the
        others.
        """
        name = f"_tkd2dcanvas_callback_{next(_command_counter)}"

        def wrapper(*args: str) -> str:
            return str(func(_make_event(self, args)))

        self.master.tk.createcommand(name, wrapper)
        self._tcl_commands.append(name)
        return name

    def _delete_command(self, name: str) -> None:
        """Deletes a registered Tcl command (the callback is gone for good)."""
        try:
            self.master.tk.deletecommand(name)
        except tkinter.TclError:
            pass  # already gone
        if name in self._tcl_commands:
            self._tcl_commands.remove(name)

    # --- lifecycle -----------------------------------------------------------------
    def destroy(self) -> None:
        for name in list(self._tcl_commands):
            self._delete_command(name)
        self._tcl_commands.clear()
        try:
            self.master.tk.call("destroy", self._path)
        except tkinter.TclError:
            pass  # already gone
