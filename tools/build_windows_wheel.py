#!/usr/bin/env python3
"""Build the Windows wheel for tkd2dcanvas.

Steps:
  1. Verify the Tcl/Tk SDK (tools/check_tcltk_dev.py).
  2. Configure & build with CMake (Release).
  3. Stage packaging/ + python/ into a temp dir and run `python -m build`.
  4. Leave the wheel under dist/.

Prerequisites:
  - third_party/tcltk-sdk populated (see third_party/tcltk-sdk/README.md)
  - cmake (>= 3.26) and Visual Studio (MSVC)
  - python -m pip install build
"""
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent


def run(cmd: list[str], **kw) -> None:
    print("+", " ".join(cmd))
    subprocess.run(cmd, check=True, **kw)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", default=str(REPO_ROOT / "third_party" / "tcltk-sdk"),
                        help="Tcl/Tk SDK root (default: third_party/tcltk-sdk)")
    parser.add_argument("--skip-sdk-check", action="store_true")
    parser.add_argument("--keep-stage", action="store_true", help="Do not delete the staging dir")
    args = parser.parse_args()

    if not args.skip_sdk_check:
        if subprocess.run([sys.executable, str(REPO_ROOT / "tools" / "check_tcltk_dev.py"), args.sdk]).returncode != 0:
            return 1

    build_dir = REPO_ROOT / "build" / "wheel"
    run(["cmake", "-S", str(REPO_ROOT), "-B", str(build_dir),
          "-DCMAKE_BUILD_TYPE=Release", "-DTKG_SYNC_PYTHON_PACKAGE=ON"])
    run(["cmake", "--build", str(build_dir), "--config", "Release"])

    dll = REPO_ROOT / "python" / "tkd2dcanvas" / "tcl" / "tkd2dcanvas" / "tkd2dcanvas01.dll"
    if not dll.is_file():
        print(f"ERROR: expected built DLL at {dll} — did the build sync the package?", file=sys.stderr)
        return 1

    with tempfile.TemporaryDirectory(prefix="tkgdp_build_") as tmp:
        stage = Path(tmp)
        shutil.copytree(REPO_ROOT / "python" / "tkd2dcanvas", stage / "tkd2dcanvas")
        shutil.copy(REPO_ROOT / "packaging" / "pyproject.toml", stage / "pyproject.toml")
        shutil.copy(REPO_ROOT / "packaging" / "MANIFEST.in", stage / "MANIFEST.in")
        shutil.copy(REPO_ROOT / "README.md", stage / "README.md")
        shutil.copy(REPO_ROOT / "LICENSE", stage / "LICENSE")
        shutil.copy(REPO_ROOT / "COPYING", stage / "COPYING")

        try:
            import build  # noqa: F401
        except ImportError:
            print("python -m build is not installed. Install it with:\n\n"
                  "    python -m pip install build\n", file=sys.stderr)
            return 1

        # The package contains a Windows DLL, so the wheel must be tagged
        # win_amd64 instead of the default py3-none-any.
        run([sys.executable, "-m", "build", "--wheel", str(stage),
              "--config-setting=--build-option=--plat-name=win_amd64",
              "--outdir", str(REPO_ROOT / "dist")])

        if args.keep_stage:
            print(f"staging kept at {stage} (temporary dir will be removed by OS)")

    print()
    print("Wheel written to dist/")
    return 0


if __name__ == "__main__":
    sys.exit(main())
