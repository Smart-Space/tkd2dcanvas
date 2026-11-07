#!/usr/bin/env python3
"""Run the Tcl test suite (tests/tcl/all.tcl) with wish/tclsh.

If no standalone Tcl/Tk is installed, fall back to the Tcl/Tk bundled with
Python (via tkinter), which is the interpreter the package is meant to run in.
"""
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
TEST_FILE = REPO_ROOT / "tests" / "tcl" / "all.tcl"


def run_with_python() -> int:
    """Run all.tcl inside the Tk interpreter bundled with Python."""
    tcl_dir = REPO_ROOT / "python" / "tkd2dcanvas" / "tcl"
    code = (
        "import tkinter, sys\n"
        "root = tkinter.Tk()\n"
        f"root.tk.eval('lappend auto_path {{{tcl_dir.as_posix()}}}')\n"
        # Mark: we are inside a windowed interpreter, so the suite must not
        # call exit (cleanupTests) at the end.
        "root.tk.eval('set ::tkg_tk_interp 1')\n"
        f"root.tk.eval('source {{{TEST_FILE.as_posix()}}}')\n"
        "root.destroy()\n"
    )
    print("Running with Python's bundled Tk (no wish/tclsh found)")
    result = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True)
    out = (result.stdout or "") + (result.stderr or "")
    print(out.strip())

    # tcltest prints one summary per file: "<file>: Total N Passed N Skipped N Failed N"
    failed = 0
    for line in out.splitlines():
        if "Failed" not in line:
            continue
        parts = line.split()
        for i, token in enumerate(parts):
            if token == "Failed" and i + 1 < len(parts) and parts[i + 1].isdigit():
                failed += int(parts[i + 1])
    if failed:
        print(f"\n{failed} test(s) failed")
        return 1
    print("\nall Tcl tests passed")
    return result.returncode


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tclsh", default=None, help="Path to tclsh/wish (default: autodetect)")
    parser.add_argument("--no-fallback", action="store_true",
                        help="Fail instead of falling back to Python's bundled Tk")
    args = parser.parse_args()

    interp = args.tclsh or shutil.which("wish") or shutil.which("tclsh")
    if not interp:
        if args.no_fallback:
            print("No wish/tclsh found on PATH.", file=sys.stderr)
            return 2
        return run_with_python()

    print(f"Running {TEST_FILE} with {interp}")
    return subprocess.run([interp, str(TEST_FILE)]).returncode


if __name__ == "__main__":
    sys.exit(main())
