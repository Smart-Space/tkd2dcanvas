"""Option serialization helpers shared by the widget wrapper and tk stubs."""

from __future__ import annotations

import re
from typing import Any, Optional

# Bare words this is safe to emit unquoted into a Tcl list. Anything else
# gets quoted properly by _brace/_quote below.
_SAFE_WORD = re.compile(r"^[A-Za-z0-9._/#+@:*,-]+$")


def serialize_value(value: Any) -> str:
    """Convert a Python value into a Tcl string argument.

    Tcl passes everything as strings; this mirrors how tkinter encodes
    booleans (1/0) and lists (``{a b c}``).

    Raises:
        ValueError: if ``value`` is bytes that are not valid UTF-8. Tcl
            arguments cross the boundary as Python ``str`` (UTF-8), so
            arbitrary binary data cannot be represented faithfully instead
            of silently emitting a ``repr()`` like ``b'\\xff'``.
    """
    if value is True:
        return "1"
    if value is False:
        return "0"
    if value is None:
        return ""
    if isinstance(value, str):
        return value
    if isinstance(value, bytes):
        try:
            return value.decode("utf-8")
        except UnicodeDecodeError as exc:
            raise ValueError(
                f"bytes option value is not valid UTF-8: {exc}"
            ) from exc
    if isinstance(value, (int, float)):
        return repr(value) if isinstance(value, float) else str(value)
    if isinstance(value, (list, tuple)):
        return " ".join(_brace(serialize_value(v)) for v in value)
    if isinstance(value, set):
        return " ".join(_brace(serialize_value(v)) for v in sorted(value, key=str))
    return str(value)


def _brace(s: str) -> str:
    if s == "":
        return "{}"
    if _SAFE_WORD.match(s):
        return s
    depth = 0
    for c in s:
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth < 0:
                return _quote(s)
    if depth:
        return _quote(s)
    return "{" + s + "}"


def _quote(s: str) -> str:
    """Quote ``s`` with backslashes inside a double-quoted word."""
    escaped = (
        s.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("$", "\\$")
        .replace("[", "\\[")
        .replace("\n", "\\n")
    )
    return '"' + escaped + '"'


def tk_option(name: str) -> str:
    """Python snake_case / hyphenated option name -> Tcl -option."""
    name = str(name)
    stripped = name[1:] if name.startswith("-") else name
    return "-" + stripped.replace("_", "-")


def flatten_options(options: Optional[dict[str, Any]]) -> list[str]:
    """{fill='red', outline='navy'} -> ['-fill', 'red', '-outline', 'navy']."""
    if options is None:
        return []
    args: list[str] = []
    for key, value in options.items():
        if value is None:
            continue
        args.append(tk_option(key))
        args.append(serialize_value(value))
    return args
