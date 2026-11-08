"""Pythonic wrapper around the tkd2dcanvas Tcl widget."""

from __future__ import annotations

import itertools
import tkinter
from typing import Any, Callable, Optional, Tuple

from ._loader import load_into
from .errors import CommandError
from .options import flatten_options, serialize_value

_command_counter = itertools.count(1)

# The substitutions Tk performs on a binding script before it is evaluated.
# The order matches tkinter's own widget bindings, so that a callback can be
# written exactly like in tkinter.
_SUBST_FORMAT = (
    "%#", "%b", "%f", "%h", "%k", "%s", "%t", "%w", "%x", "%y",
    "%A", "%E", "%K", "%N", "%W", "%T", "%X", "%Y", "%D",
)
_SUBST_FORMAT_STR = " ".join(_SUBST_FORMAT)


def _parse_id_list(text: str) -> tuple[int, ...]:
    ids: list[int] = []
    for tok in text.split():
        try:
            ids.append(int(tok))
        except ValueError:
            pass
    return tuple(ids)


def _parse_bbox(text: str) -> Optional[Tuple[float, float, float, float]]:
    parts = text.split()
    if len(parts) != 4:
        return None
    try:
        return (float(parts[0]), float(parts[1]), float(parts[2]), float(parts[3]))
    except ValueError:
        return None


def _make_event(canvas: "Canvas", args: Any) -> Any:
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


class Canvas(tkinter.Canvas):
    """Direct2D canvas widget; API-compatible subset of tkinter.Canvas.

    Coordinates follow Tk conventions. ``create_*`` methods accept either
    ``x1, y1, x2, y2`` arguments or a flat ``*coords`` sequence.
    """

    def __init__(self, master: Optional[tkinter.Misc] = None, cnf: Optional[dict] = None, **kw: Any) -> None:
        if master is None:
            master = tkinter._get_default_root()  # noqa: SLF001 - tkinter convention
            if master is None:
                raise RuntimeError(
                    "no Tk root exists; create one with tkinter.Tk() first"
                )
        if kw:
            cnf = {**(cnf or {}), **kw}
        else:
            cnf = dict(cnf or {})
        self.master = master
        load_into(master)

        # Tcl commands registered for Python callbacks (see _register_callback).
        self._tcl_commands: list[str] = []
        self.widgetName = "tkd2dcanvas"

        if "name" not in cnf:
            stem = "tkd2dcanvas"
            if master._last_child_ids is None:
                master._last_child_ids = {}
            count = master._last_child_ids.get(stem, 0) + 1
            master._last_child_ids[stem] = count
            cnf["name"] = f"!{stem}" if count == 1 else f"!{stem}{count}"
        try:
            tkinter.BaseWidget._setup(self, master, cnf)  # noqa: SLF001
        except (KeyError, AttributeError) as exc:
            raise CommandError(f"invalid master for tkd2dcanvas: {exc}") from exc

        self._tclCommands = self._tcl_commands

        options = self._flatten_options(cnf)
        try:
            path = str(self.master.tk.call("tkd2dcanvas", self._w, *options))
        except tkinter.TclError as exc:
            self._unregister_from_master()
            raise CommandError(f"failed to create tkd2dcanvas widget: {exc}") from exc
        if path != self._w:
            self._unregister_from_master()
            try:
                self.master.tk.call("destroy", path)
            except tkinter.TclError:
                pass
            raise CommandError(
                f"tkd2dcanvas widget created at unexpected path {path!r}"
            )

    def _unregister_from_master(self) -> None:
        """Removes the master.children entry _setup() made for us."""
        sibling = self.master.children.get(self._name)
        if sibling is self:
            del self.master.children[self._name]

    # --- internals ----------------------------------------------------------
    @property
    def tk_path(self) -> str:
        return self._w

    def _call(self, *args: Any) -> str:
        try:
            result = self.master.tk.call(self._w, *args)
        except tkinter.TclError as exc:
            raise CommandError(str(exc)) from exc
        if isinstance(result, tuple):
            # A Tcl list comes back as a tuple of strings.
            return " ".join(str(part) for part in result)
        if not isinstance(result, str):
            return str(result)
        return result

    def _report_dict(self, *args: Any) -> dict[str, Any]:
        """Reads a no-argument configure/itemconfigure report.

        The native widget returns a Tcl dict; the empty string is returned
        by ``itemconfigure`` when no item matches. Values are converted to
        the Python type the Tcl runtime reports (int/float/str); anything
        else is stringified. Option keys are returned without the leading
        dash.
        """
        try:
            result = self.master.tk.call(*args)
        except tkinter.TclError as exc:
            raise CommandError(str(exc)) from exc
        if isinstance(result, str) and not result.strip():
            return {}
        try:
            keys = self.master.tk.call("dict", "keys", result)
        except tkinter.TclError as exc:
            raise CommandError(f"unexpected report format: {result!r}") from exc
        report: dict[str, Any] = {}
        for key in keys:
            name = str(key)
            value = self.master.tk.call("dict", "get", result, key)
            if not isinstance(value, (str, int, float)):
                value = str(value)
            report[name[1:] if name.startswith("-") else name] = value
        return report

    @staticmethod
    def _coords(args: tuple) -> list[Any]:
        # Accept both create_rectangle(x1, y1, x2, y2) and a single flat list.
        if len(args) == 1 and isinstance(args[0], (list, tuple)):
            return list(args[0])
        return list(args)

    # --- item creation -------------------------------------------------------
    def create(self, itemtype: str, *coords: Any, **kw: Any) -> int:
        args = [serialize_value(c) for c in coords]
        args += self._flatten_options(kw)
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

    # --- coordinates ----------------------------------------------------------
    def coords(self, item: Any, *args: Any):
        result = self._call("coords", serialize_value(item), *[serialize_value(a) for a in args])
        if args:
            return None
        return [float(v) for v in result.split()]

    def move(self, items: Any, dx: float, dy: float) -> None:
        self._call("move", serialize_value(items), serialize_value(dx), serialize_value(dy))

    def moveto(self, items: Any, x: Any, y: Any) -> None:
        """Moves the items so that the upper-left corner of the bbox of the
        first matching item is located at ``(x, y)``.

        ``x`` or ``y`` may be the empty string, in which case the
        corresponding coordinate is left unchanged (Tk's ``moveto``).
        """
        self._call("moveto", serialize_value(items), serialize_value(x), serialize_value(y))

    def delete(self, *items: Any) -> None:
        self._call("delete", *[serialize_value(i) for i in items])

    # --- options ----------------------------------------------------------------
    def itemconfigure(self, item: Any, cnf: Optional[dict] = None, **kw: Any):
        if isinstance(cnf, str):
            # itemconfigure(item, "width") — single option query.
            report = self._report_dict(self._w, "itemconfigure", serialize_value(item))
            if cnf.lstrip("-") not in report:
                raise CommandError(f'unknown option "{cnf}"')
            return report[cnf.lstrip("-")]
        if cnf is not None or kw:
            options = {**(cnf or {}), **kw}
            self._call("itemconfigure", serialize_value(item), *self._flatten_options(options))
            return None
        return self._report_dict(self._w, "itemconfigure", serialize_value(item))

    itemconfig = itemconfigure

    def itemcget(self, item: Any, option: str) -> str:
        return self._call("itemcget", serialize_value(item), f"-{option.lstrip('-')}")

    def configure(self, cnf: Optional[dict] = None, **kw: Any):
        if isinstance(cnf, str):
            # configure("width") — single option query.
            report = self._report_dict(self._w, "configure")
            if cnf.lstrip("-") not in report:
                raise CommandError(f'unknown option "{cnf}"')
            return report[cnf.lstrip("-")]
        if cnf is not None or kw:
            options = {**(cnf or {}), **kw}
            self._call("configure", *self._flatten_options(options))
            return None
        return self._report_dict(self._w, "configure")

    config = configure

    def keys(self) -> list[str]:
        """Option names of the widget, without the leading dash."""
        return list(self.configure())

    def cget(self, option: str) -> str:
        return self._call("cget", f"-{option.lstrip('-')}")

    # --- bbox / find / tags -----------------------------------------------------
    def bbox(self, *items: Any) -> Optional[Tuple[float, float, float, float]]:
        return _parse_bbox(self._call("bbox", *[serialize_value(i) for i in items]))

    def find_withtag(self, tag: Any) -> tuple[int, ...]:
        return _parse_id_list(self._call("find", "withtag", serialize_value(tag)))

    def find_all(self) -> tuple[int, ...]:
        return _parse_id_list(self._call("find", "all"))

    def find_closest(self, x: float, y: float, halo: Optional[float] = None,
                     start: Any = None) -> tuple[int, ...]:
        args = [serialize_value(x), serialize_value(y)]
        if halo is not None:
            args.append(serialize_value(halo))
        if start is not None:
            args.append(serialize_value(start))
        return _parse_id_list(self._call("find", "closest", *args))

    def find_overlapping(self, x1: float, y1: float, x2: float, y2: float) -> tuple[int, ...]:
        return _parse_id_list(
            self._call("find", "overlapping", serialize_value(x1), serialize_value(y1),
                       serialize_value(x2), serialize_value(y2))
        )

    def find_enclosed(self, x1: float, y1: float, x2: float, y2: float) -> tuple[int, ...]:
        return _parse_id_list(
            self._call("find", "enclosed", serialize_value(x1), serialize_value(y1),
                       serialize_value(x2), serialize_value(y2))
        )

    def addtag_withtag(self, tag: Any, tagOrId: Any) -> None:
        self._call("addtag", serialize_value(tag), "withtag", serialize_value(tagOrId))

    def addtag_all(self, tag: Any) -> None:
        self._call("addtag", serialize_value(tag), "all")

    def addtag_above(self, tag: Any, tagOrId: Any) -> None:
        self._call("addtag", serialize_value(tag), "above", serialize_value(tagOrId))

    def addtag_below(self, tag: Any, tagOrId: Any) -> None:
        self._call("addtag", serialize_value(tag), "below", serialize_value(tagOrId))

    def addtag_closest(self, tag: Any, x: float, y: float, halo: Any = None,
                       start: Any = None) -> None:
        args = ["closest", serialize_value(x), serialize_value(y)]
        if halo is not None:
            args.append(serialize_value(halo))
        if start is not None:
            args.append(serialize_value(start))
        self._call("addtag", serialize_value(tag), *args)

    def addtag_overlapping(self, tag: Any, x1: float, y1: float, x2: float, y2: float) -> None:
        self._call("addtag", serialize_value(tag), "overlapping", serialize_value(x1),
                   serialize_value(y1), serialize_value(x2), serialize_value(y2))

    def addtag_enclosed(self, tag: Any, x1: float, y1: float, x2: float, y2: float) -> None:
        self._call("addtag", serialize_value(tag), "enclosed", serialize_value(x1),
                   serialize_value(y1), serialize_value(x2), serialize_value(y2))

    def dtag(self, item: Any, tag: Any = None) -> None:
        if tag is None:
            self._call("dtag", serialize_value(item))
        else:
            self._call("dtag", serialize_value(item), serialize_value(tag))

    def gettags(self, item: Any) -> tuple[str, ...]:
        return tuple(self._call("gettags", serialize_value(item)).split())

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

    def scan_mark(self, x: float, y: float) -> None:
        """Records ``x`` and ``y`` and the current view (like tkinter)."""
        self._call("scan", "mark", int(x), int(y))

    def scan_dragto(self, x: float, y: float, gain: int = 10) -> None:
        """Moves the view by ``gain`` times the pointer distance since the mark."""
        self._call("scan", "dragto", int(x), int(y), int(gain))

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
        return self._bind((self._w, "bind", serialize_value(tag)),
                           sequence, func, add)

    def bind(self, sequence: Optional[str] = None, func: Optional[Any] = None,
             add: Optional[bool] = None) -> Any:
        """Binds an event sequence to the canvas widget itself.

        Like tkinter, this is the *widget* binding; use :meth:`tag_bind` for
        the items of the canvas.  See :meth:`tag_bind` for the meaning of
        ``func`` and ``add``.
        """
        return self._bind(("bind", self._w), sequence, func, add)

    def tag_unbind(self, tag: Any, sequence: str, funcid: Optional[str] = None) -> None:
        """Removes the binding of ``sequence`` for a tag (or a single item).

        With ``funcid`` only that single callback is removed, otherwise the
        whole binding of the sequence goes away.
        """
        self._unbind((self._w, "bind", serialize_value(tag), sequence), funcid)

    def unbind(self, sequence: str, funcid: Optional[str] = None) -> None:
        """Removes the widget binding of ``sequence``.

        With ``funcid`` only that single callback is removed, otherwise the
        whole binding of the sequence goes away.
        """
        self._unbind(("bind", self._w, sequence), funcid)

    def _bind(self, what: tuple, sequence: Optional[str], func: Any,
              add: Optional[bool]) -> Any:
        """The tkinter binding protocol (see tkinter.Misc._bind)."""
        if isinstance(func, str):
            # A plain Tcl script replaces the whole binding.
            self.master.tk.call(what + (sequence, func))
            return None
        if func:
            funcid = self._register_callback(func)
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

    def _register_callback(self, func: Callable[..., Any]) -> str:
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

    def _register_command(self, func: Callable[..., Any]) -> str:
        """Registers a Python callable as a Tcl command receiving the raw
        string arguments, for use as a script prefix (e.g. a scroll command).
        tkinter does the same for callables in option values.
        """
        name = f"_tkd2dcanvas_callback_{next(_command_counter)}"

        def wrapper(*args: str) -> str:
            result = func(*args)
            return "" if result is None else str(result)

        self.master.tk.createcommand(name, wrapper)
        self._tcl_commands.append(name)
        return name

    def _flatten_options(self, options: dict) -> list[str]:
        """Flattens like flatten_options, registering Python callables as Tcl
        commands (tkinter's callable-in-option convention)."""
        resolved = dict(options)
        for key, value in resolved.items():
            if callable(value):
                resolved[key] = self._register_command(value)
        return flatten_options(resolved)

    def _delete_command(self, name: str) -> None:
        """Deletes a registered Tcl command (the callback is gone for good)."""
        try:
            self.master.tk.deletecommand(name)
        except tkinter.TclError:
            pass  # already gone
        if name in self._tcl_commands:
            self._tcl_commands.remove(name)

    def addtag(self, *args: Any) -> None:
        self._call("addtag", *[serialize_value(a) for a in args])

    def find(self, *args: Any) -> tuple[int, ...]:
        return _parse_id_list(self._call("find", *[serialize_value(a) for a in args]))

    def focus(self, *args: Any) -> Any:
        """Set or query the canvas item that has the keyboard focus.

        Tk semantics (item ``focus``): with no argument, return the focus
        item's id (``None`` when none has the focus). Otherwise set the
        focus to the first matching item that supports the insertion cursor
        (currently only text items — the native package has none yet, so a
        non-empty selection is silently ignored, like Tk), or reset it when
        ``args[0]`` is the empty string. Keyboard events are directed to
        the focus item only.
        """
        if not args:
            result = self._call("focus")
            return int(result) if result else None
        if len(args) != 1:
            raise CommandError("focus takes at most one argument")
        self._call("focus", serialize_value(args[0]))
        return None

    def find_above(self, item: Any) -> tuple[int, ...]:
        return _parse_id_list(self._call("find", "above", serialize_value(item)))

    def find_below(self, item: Any) -> tuple[int, ...]:
        return _parse_id_list(self._call("find", "below", serialize_value(item)))

    def findwithtag(self, tag: Any) -> tuple[int, ...]:
        return self.find_withtag(tag)

    def findall(self) -> tuple[int, ...]:
        return self.find_all()

    def findclosest(self, x: float, y: float, halo: Any = None,
                    start: Any = None) -> tuple[int, ...]:
        return self.find_closest(x, y, halo, start)

    def tagbind(self, tag: Any, sequence: Any = None, func: Any = None,
                add: Any = None) -> Any:
        return self.tag_bind(tag, sequence, func, add)

    def tagunbind(self, tag: Any, sequence: str, funcid: Any = None) -> None:
        self.tag_unbind(tag, sequence, funcid)

    # --- scaling / coordinate mapping / item type -----------------------------------
    def scale(self, tagOrId: Any, xorigin: float, yorigin: float,
              xscale: float, yscale: float) -> None:
        """Scales the coordinates of all matching items (Tk's ``scale``)."""
        self._call("scale", serialize_value(tagOrId), serialize_value(xorigin),
                   serialize_value(yorigin), serialize_value(xscale),
                   serialize_value(yscale))

    def canvasx(self, screenx: Any, gridspacing: Any = None) -> float:
        """Maps a window x-coordinate to a canvas x-coordinate (Tk semantics).

        With ``gridspacing`` > 0 the result is snapped to the nearest multiple
        of it (halves away from zero).
        """
        args = [serialize_value(screenx)]
        if gridspacing is not None:
            args.append(serialize_value(gridspacing))
        return float(self._call("canvasx", *args))

    def canvasy(self, screeny: Any, gridspacing: Any = None) -> float:
        """Maps a window y-coordinate to a canvas y-coordinate (Tk semantics).

        With ``gridspacing`` > 0 the result is snapped to the nearest multiple
        of it (halves away from zero).
        """
        args = [serialize_value(screeny)]
        if gridspacing is not None:
            args.append(serialize_value(gridspacing))
        return float(self._call("canvasy", *args))

    def type(self, tagOrId: Any) -> Optional[str]:
        """Returns the type of the first matching item, or None if there is no match."""
        result = self._call("type", serialize_value(tagOrId))
        return result if result else None

    lift = tkraise = tag_raise
    lower = tag_lower

    def _unimplemented(self, name: str, milestone: str) -> Any:
        raise NotImplementedError(
            f"Canvas.{name} is not implemented yet "
            f"({milestone}); see docs/tk_canvas_compat_matrix.md"
        )

    def create_text(self, *args: Any, **kw: Any) -> int:
        """Create a text item at ``x, y`` (or a single ``(x, y)`` pair).

        Supports the Tk canvas text options: ``-text``, ``-font``,
        ``-fill``, ``-anchor``, ``-justify`` (left/center/right), ``-width``
        (word-wrap length in pixels; 0 disables wrapping) and ``-wrap``
        (word/char/none), plus ``-tags``.
        """
        return self.create("text", *self._coords(args), **kw)

    def create_image(self, *args: Any, **kw: Any) -> int:
        """Create an image item at ``x, y`` referencing a Tk photo image.

        The ``image`` option names a Tk photo image (e.g. a
        :class:`tkinter.PhotoImage`, which serializes to its registered
        name). Pixels are copied when the option is applied; later edits
        to the photo are not reflected until the option is configured
        again.
        """
        return self.create("image", *self._coords(args), **kw)

    def create_bitmap(self, *args: Any, **kw: Any) -> int:
        """Create a bitmap item at ``x, y`` from an XBM bitmap.

        ``bitmap`` is either one of Tk's predefined names
        (``error``/``gray12``/``gray25``/``gray50``/``gray75``/``hourglass``/
        ``info``/``questhead``/``question``/``warning``) or ``@file.xbm``.
        The ``foreground``/``background`` colors paint the set/clear bits;
        an empty background means transparent.
        """
        return self.create("bitmap", *self._coords(args), **kw)

    # --- not implemented (see docs/tk_canvas_compat_matrix.md) --------------------
    def create_window(self, *args: Any, **kw: Any) -> int:
        self._unimplemented("create_window", "milestone M10")

    def dchars(self, *args: Any):
        self._unimplemented("dchars", "text item 编辑命令未实现")

    def icursor(self, *args: Any):
        self._unimplemented("icursor", "text item 编辑命令未实现")

    def index(self, *args: Any):
        self._unimplemented("index", "text item 编辑命令未实现")

    def insert(self, *args: Any):
        self._unimplemented("insert", "text item 编辑命令未实现")

    def postscript(self, *args: Any, **kw: Any):
        self._unimplemented("postscript", "明确不支持（打印路径不计划）")

    def select_adjust(self, *args: Any):
        self._unimplemented("select_adjust", "selection item 未实现")

    def select_clear(self, *args: Any):
        self._unimplemented("select_clear", "selection item 未实现")

    def select_from(self, *args: Any):
        self._unimplemented("select_from", "selection item 未实现")

    def select_item(self, *args: Any):
        self._unimplemented("select_item", "selection item 未实现")

    def select_to(self, *args: Any):
        self._unimplemented("select_to", "selection item 未实现")

    # --- lifecycle -----------------------------------------------------------------
    def destroy(self) -> None:
        for name in list(self._tcl_commands):
            self._delete_command(name)
        self._tcl_commands.clear()
        try:
            self.master.tk.call("destroy", self._w)
        except tkinter.TclError:
            pass  # already gone
        self._unregister_from_master()
        tkinter.Misc.destroy(self)
