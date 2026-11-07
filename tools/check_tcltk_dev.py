#!/usr/bin/env python3
"""Validate a Tcl/Tk development SDK before building tkd2dcanvas.

Usage:
    python tools/check_tcltk_dev.py [SDK_ROOT]

Default SDK_ROOT: third_party/tcltk-sdk (repo-root relative).
Checks: header presence (including the Windows platform headers and the
X11 compatibility headers Tk needs), stub libraries, expected 8.6 version,
MSVC compiler availability (informational). Exits 0 when usable, 1 otherwise.
"""
from __future__ import annotations

import argparse
import re
import shutil
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SDK = REPO_ROOT / "third_party" / "tcltk-sdk"

REQUIRED = ["tcl.h", "tk.h", "tclDecls.h", "tkDecls.h"]
REQUIRED_PLATFORM = ["tclPlatDecls.h", "tkPlatDecls.h"]
REQUIRED_X11 = ["X11/Xlib.h", "X11/X.h", "X11/Xutil.h", "X11/keysym.h"]
STUBS = ["tclstub86.lib", "tkstub86.lib"]


def check_headers(include_dir: Path) -> tuple[list[str], dict[str, str]]:
    problems: list[str] = []
    versions: dict[str, str] = {}

    for name in REQUIRED + REQUIRED_PLATFORM:
        if not (include_dir / name).is_file():
            problems.append(f"missing header: {include_dir / name}")

    for rel in REQUIRED_X11:
        if not (include_dir / rel).is_file():
            problems.append(f"missing X11 compatibility header: {include_dir / rel}")

    tcl_h = include_dir / "tcl.h"
    if tcl_h.is_file():
        text = tcl_h.read_text(errors="ignore")
        for pattern, label in ((r"#\s*define\s+TCL_MAJOR_VERSION\s+(\d+)", "major"),
                               (r"#\s*define\s+TCL_MINOR_VERSION\s+(\d+)", "minor"),
                               (r"#\s*define\s+TCL_PATCH_LEVEL\s+\"?([^\"\s]+)\"?", "patch")):
            m = re.search(pattern, text)
            if m:
                versions[label] = m.group(1)
    return problems, versions


def check_libs(lib_dir: Path) -> list[str]:
    problems: list[str] = []
    for name in STUBS:
        if not (lib_dir / name).is_file():
            problems.append(f"missing stub library: {lib_dir / name}")
    return problems


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sdk", nargs="?", default=str(DEFAULT_SDK),
                        help="SDK root containing include/ and lib/ (default: third_party/tcltk-sdk)")
    args = parser.parse_args()

    sdk = Path(args.sdk)
    print(f"Checking Tcl/Tk SDK at: {sdk}")

    # Layout: either <sdk>/include + <sdk>/lib, or files directly in one dir.
    include_dir = sdk / "include" if (sdk / "include" / "tcl.h").is_file() else sdk
    lib_dir = sdk / "lib" if any((sdk / "lib").glob("*stub*.lib")) else sdk

    problems, versions = check_headers(include_dir)
    problems += check_libs(lib_dir)

    major = versions.get("major")
    minor = versions.get("minor")
    if major and minor:
        print(f"Detected Tcl version: {major}.{minor} (patch: {versions.get('patch', '?')})")
        if (major, minor) != ("8", "6"):
            print("WARNING: this SDK is not Tcl/Tk 8.6. tkd2dcanvas targets the\n"
                  "         Tcl/Tk version bundled with Python on Windows (8.6).")
    else:
        print("WARNING: could not parse TCL_MAJOR_VERSION/TCL_MINOR_VERSION from tcl.h")

    print("Compiler: ", end="")
    if shutil.which("cl"):
        print("MSVC cl.exe found on PATH")
    else:
        print("cl.exe not on PATH - run inside 'x64 Native Tools Command Prompt for VS'")
        print("          or use cmake CMakePresets (auto-detects Visual Studio).")

    if problems:
        print()
        for p in problems:
            print("FAIL:", p)
        print()
        print("SDK incomplete. See third_party/tcltk-sdk/README.md for how to populate it.")
        return 1

    print()
    print("OK: SDK looks usable. Next: cmake --preset windows-release && cmake --build build/release --config Release")
    return 0


if __name__ == "__main__":
    sys.exit(main())
