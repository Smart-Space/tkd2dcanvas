"""Option serialization helpers shared by the widget wrapper and tk stubs."""

from __future__ import annotations

from typing import Any


def serialize_value(value: Any) -> str:
    """Convert a Python value into a Tcl string argument.

    Tcl passes everything as strings; this mirrors how tkinter encodes
    booleans (1/0) and lists ({a b c}).
    """
    if value is True:
        return "1"
    if value is False:
        return "0"
    if value is None:
        return ""
    if isinstance(value, str):
        return value
    if isinstance(value, bool):  # pragma: no cover - handled above
        return "1" if value else "0"
    if isinstance(value, (int, float)):
        return repr(value) if isinstance(value, float) else str(value)
    if isinstance(value, (list, tuple)):
        return " ".join(_brace(serialize_value(v)) for v in value)
    return str(value)


def _brace(s: str) -> str:
    if s == "" or any(c in s for c in ' \t\n"{}[]'):
        escaped = s.replace("\\", "\\\\").replace("}", "\\}")
        return "{" + escaped + "}"
    return s


def tk_option(name: str) -> str:
    """Python snake_case / hyphenated option name -> Tcl -option."""
    return "-" + name.replace("_", "-")


def flatten_options(options: dict[str, Any]) -> list[str]:
    """{fill='red', outline='navy'} -> ['-fill', 'red', '-outline', 'navy']."""
    args: list[str] = []
    for key, value in options.items():
        if value is None:
            continue
        args.append(tk_option(key))
        args.append(serialize_value(value))
    return args
