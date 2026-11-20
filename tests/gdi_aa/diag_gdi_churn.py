"""diag_gdi_churn.py -- does GDI object churn grow the process' commit?

The CRT heap reuses its freed blocks (diag_heap_reuse.py), and the canvas
module has no live bytes left when the process exits, yet the private
commit of the process grows while the canvas repaints.  The remaining
suspect is gdi32's own heap: the anti-aliased renderer creates and destroys
GDI objects (pens, brushes, regions, blend buffers) for every shape.

This script does only that - create and destroy GDI objects - and watches
the private commit.

Usage:
    python tests/gdi_aa/diag_gdi_churn.py --rounds 800 --perround 200
"""

from __future__ import annotations

import argparse
import ctypes
import ctypes.wintypes as wt
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "python"))

K32 = ctypes.WinDLL("kernel32", use_last_error=True)
PSAPI = ctypes.WinDLL("psapi", use_last_error=True)
GDI32 = ctypes.WinDLL("gdi32", use_last_error=True)
USER32 = ctypes.WinDLL("user32", use_last_error=True)
K32.GetCurrentProcess.restype = wt.HANDLE
USER32.GetGuiResources.argtypes = [wt.HANDLE, wt.DWORD]
USER32.GetGuiResources.restype = wt.DWORD


class C(ctypes.Structure):
    _fields_ = [
        ("cb", wt.DWORD), ("PageFaultCount", wt.DWORD),
        ("PeakWorkingSetSize", ctypes.c_size_t), ("WorkingSetSize", ctypes.c_size_t),
        ("QuotaPeakPagedPoolUsage", ctypes.c_size_t), ("QuotaPagedPoolUsage", ctypes.c_size_t),
        ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t), ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
        ("PagefileUsage", ctypes.c_size_t), ("PeakPagefileUsage", ctypes.c_size_t),
        ("PrivateUsage", ctypes.c_size_t),
    ]


PSAPI.GetProcessMemoryInfo.argtypes = [wt.HANDLE, ctypes.POINTER(C), wt.DWORD]
GDI32.CreateSolidBrush.argtypes = [wt.DWORD]
GDI32.CreateSolidBrush.restype = wt.HGDIOBJ
GDI32.DeleteObject.argtypes = [wt.HGDIOBJ]
GDI32.CreateRectRgn.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int]
GDI32.CreateRectRgn.restype = wt.HGDIOBJ
GDI32.CreateCompatibleDC.argtypes = [wt.HDC]
GDI32.CreateCompatibleDC.restype = wt.HDC
GDI32.DeleteDC.argtypes = [wt.HDC]


def priv() -> int:
    c = C()
    c.cb = ctypes.sizeof(c)
    PSAPI.GetProcessMemoryInfo(K32.GetCurrentProcess(), ctypes.byref(c), c.cb)
    return c.PrivateUsage


def gdi_count() -> int:
    return USER32.GetGuiResources(K32.GetCurrentProcess(), 0)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rounds", type=int, default=800)
    ap.add_argument("--perround", type=int, default=200)
    args = ap.parse_args()

    base = priv()
    print("start priv={0:.0f} KB gdi={1}".format(base / 1024, gdi_count()),
          flush=True)

    for r in range(1, args.rounds + 1):
        for i in range(args.perround):
            brush = GDI32.CreateSolidBrush(0x004080C0)
            if brush:
                GDI32.DeleteObject(brush)
            region = GDI32.CreateRectRgn(i % 100, 0, (i % 100) + 60, 60)
            if region:
                GDI32.DeleteObject(region)
        if r % 100 == 0 or r == 1:
            now = priv()
            print("round {0:5d} priv={1:8.0f} KB  d={2:+9.0f} KB  gdi={3}".format(
                r, now / 1024, (now - base) / 1024, gdi_count()), flush=True)


if __name__ == "__main__":
    main()
