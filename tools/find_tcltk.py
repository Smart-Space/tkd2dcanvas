#!/usr/bin/env python3
"""Locate Tcl/Tk development files (headers + stub libraries).

Search order:
  1. explicit --root paths (repeatable)
  2. TCLTK_ROOT / TCL_ROOT / TK_ROOT environment variables
  3. in-repo SDK: third_party/tcltk-sdk
  4. common Windows locations

Prints a report and exits 0 when everything needed was found, else 1.
"""
from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
SKIP_DIRS = {".git", "node_modules", "Windows", "$Recycle.Bin", "System Volume Information"}
REQUIRED_HEADERS = ["tcl.h", "tk.h"]
STUB_PATTERNS = {"tcl": ["tclstub86.lib", "tclstub87.lib", "tclstub90.lib"],
                 "tk": ["tkstub86.lib", "tkstub87.lib", "tkstub90.lib"]}


def candidate_roots(explicit: list[str]) -> list[Path]:
    roots: list[Path] = [Path(r) for r in explicit]
    for var in ("TCLTK_ROOT", "TCL_ROOT", "TK_ROOT"):
        if os.environ.get(var):
            roots.append(Path(os.environ[var]))
    roots.append(REPO_ROOT / "third_party" / "tcltk-sdk")
    for base in ("C:/", "D:/"):
        for name in ("Tcl", "tcl", "TclTk"):
            p = Path(base) / name
            if p.is_dir():
                roots.append(p)
    pf = os.environ.get("ProgramFiles")
    if pf:
        p = Path(pf) / "Tcl"
        if p.is_dir():
            roots.append(p)
    return roots


def scan(root: Path, max_depth: int = 6):
    headers: dict[str, Path] = {}
    stubs: dict[str, Path] = {}
    root = root.resolve()

    def walk(dir_path: Path, depth: int):
        if depth > max_depth:
            return
        try:
            with os.scandir(dir_path) as it:
                entries = list(it)
        except OSError:
            return
        for entry in entries:
            try:
                if entry.is_dir(follow_symlinks=False):
                    if entry.name in SKIP_DIRS:
                        continue
                    walk(Path(entry.path), depth + 1)
                    continue
            except OSError:
                continue
            name = entry.name.lower()
            full = Path(entry.path)
            if name in REQUIRED_HEADERS and name not in headers:
                headers[name] = full
            for kind, names in STUB_PATTERNS.items():
                if name.lower() == kind + "stub86.lib" or name.lower() in names:
                    stubs.setdefault(kind, full)
                elif name.lower().startswith(kind + "stub") and name.lower().endswith(".lib"):
                    stubs.setdefault(kind, full)

    if root.is_dir():
        walk(root, 0)
    return headers, stubs


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", action="append", default=[],
                        help="Root to search (repeatable); default: sdk + env + common paths")
    parser.add_argument("--json", action="store_true", help="Emit JSON report")
    args = parser.parse_args()

    report = []
    ok = False
    for root in candidate_roots(args.root):
        headers, stubs = scan(root)
        entry = {
            "root": str(root),
            "headers": {k: str(v) for k, v in headers.items()},
            "stubs": {k: str(v) for k, v in stubs.items()},
        }
        missing = [h for h in REQUIRED_HEADERS if h not in headers] + \
                  [k for k in ("tcl", "tk") if k not in stubs]
        entry["complete"] = not missing
        entry["missing"] = missing
        report.append(entry)
        ok = ok or entry["complete"]

    if args.json:
        print(json.dumps(report, indent=2))
    else:
        for entry in report:
            status = "OK " if entry["complete"] else "MISS"
            print(f"[{status}] {entry['root']}")
            if not entry["complete"]:
                print(f"       missing: {', '.join(entry['missing'])}")
        print()
        print("Result:", "found a complete SDK" if ok else "no complete SDK found")

    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
