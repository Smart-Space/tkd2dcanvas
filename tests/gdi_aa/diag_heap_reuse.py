"""diag_heap_reuse.py -- does the CRT heap reuse the freed blocks?

The canvas' private commit grows while it repaints, although the module
itself has no live bytes left at the end (see the allocation probe).  That
points at the allocator: the freed blocks are not reused, so the heap's
commit grows with the churn.

This script does the same thing *without* the canvas: it allocates and
frees blocks of the sizes the anti-aliased renderer uses and watches the
process' private commit.

Usage:
    python tests/gdi_aa/diag_heap_reuse.py --rounds 800 --blocks 200
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
LIBC = ctypes.CDLL("msvcrt")
K32.GetCurrentProcess.restype = wt.HANDLE


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
LIBC.malloc.argtypes = [ctypes.c_size_t]
LIBC.malloc.restype = ctypes.c_void_p
LIBC.free.argtypes = [ctypes.c_void_p]


def priv() -> int:
    c = C()
    c.cb = ctypes.sizeof(c)
    PSAPI.GetProcessMemoryInfo(K32.GetCurrentProcess(), ctypes.byref(c), c.cb)
    return c.PrivateUsage


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rounds", type=int, default=800)
    ap.add_argument("--blocks", type=int, default=200)
    ap.add_argument("--sizes", default="1024,512,256,128")
    args = ap.parse_args()

    sizes = [int(s) for s in args.sizes.split(",")]
    base = priv()
    print("start priv={0:.0f} KB".format(base / 1024), flush=True)

    for r in range(1, args.rounds + 1):
        ptrs = []
        for i in range(args.blocks):
            n = sizes[i % len(sizes)]
            p = LIBC.malloc(n)
            if p:
                ptrs.append(p)
                ctypes.memset(p, 0x41, n)
        for p in ptrs:
            LIBC.free(ctypes.c_void_p(p))
        if r % 100 == 0 or r == 1:
            now = priv()
            print("round {0:5d} priv={1:8.0f} KB  d={2:+9.0f} KB".format(
                r, now / 1024, (now - base) / 1024), flush=True)


if __name__ == "__main__":
    main()
