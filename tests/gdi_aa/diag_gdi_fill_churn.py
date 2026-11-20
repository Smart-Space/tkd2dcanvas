"""diag_gdi_fill_churn.py -- does FillRect with a per-call brush grow the commit?

The GDI renderer fills the background and every pixel aligned rectangle with
a brush that it creates and deletes for each call.  This script repeats
exactly that and watches the private commit.

Usage:
    python tests/gdi_aa/diag_gdi_fill_churn.py --rounds 400 --perround 30
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


class RECT(ctypes.Structure):
    _fields_ = [("left", ctypes.c_long), ("top", ctypes.c_long),
                ("right", ctypes.c_long), ("bottom", ctypes.c_long)]


class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [
        ("biSize", wt.DWORD), ("biWidth", ctypes.c_long),
        ("biHeight", ctypes.c_long), ("biPlanes", wt.WORD),
        ("biBitCount", wt.WORD), ("biCompression", wt.DWORD),
        ("biSizeImage", wt.DWORD), ("biXPelsPerMeter", ctypes.c_long),
        ("biYPelsPerMeter", ctypes.c_long), ("biClrUsed", wt.DWORD),
        ("biClrImportant", wt.DWORD),
    ]


class BITMAPINFO(ctypes.Structure):
    _fields_ = [("bmiHeader", BITMAPINFOHEADER), ("bmiColors", wt.DWORD * 3)]


PSAPI.GetProcessMemoryInfo.argtypes = [wt.HANDLE, ctypes.POINTER(C), wt.DWORD]
GDI32.CreateCompatibleDC.argtypes = [wt.HDC]
GDI32.CreateCompatibleDC.restype = wt.HDC
GDI32.DeleteDC.argtypes = [wt.HDC]
GDI32.CreateDIBSection.argtypes = [wt.HDC, ctypes.POINTER(BITMAPINFO), wt.UINT,
                                    ctypes.POINTER(ctypes.c_void_p), wt.HANDLE, wt.DWORD]
GDI32.CreateDIBSection.restype = wt.HBITMAP
GDI32.DeleteObject.argtypes = [wt.HGDIOBJ]
GDI32.SelectObject.argtypes = [wt.HDC, wt.HGDIOBJ]
GDI32.SelectObject.restype = wt.HGDIOBJ
GDI32.CreateSolidBrush.argtypes = [wt.DWORD]
GDI32.CreateSolidBrush.restype = wt.HGDIOBJ
USER32.FillRect.argtypes = [wt.HDC, ctypes.POINTER(RECT), wt.HBRUSH]
USER32.FillRect.restype = wt.BOOL
GDI32.IntersectClipRect.argtypes = [wt.HDC, ctypes.c_int, ctypes.c_int,
                                     ctypes.c_int, ctypes.c_int]
GDI32.SelectClipRgn.argtypes = [wt.HDC, wt.HGDIOBJ]
USER32.GetDC.argtypes = [wt.HWND]
USER32.GetDC.restype = wt.HDC


def priv() -> int:
    c = C()
    c.cb = ctypes.sizeof(c)
    PSAPI.GetProcessMemoryInfo(K32.GetCurrentProcess(), ctypes.byref(c), c.cb)
    return c.PrivateUsage


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rounds", type=int, default=400)
    ap.add_argument("--perround", type=int, default=30)
    ap.add_argument("--clip", type=int, default=1)
    args = ap.parse_args()

    screen = USER32.GetDC(None)
    dc = GDI32.CreateCompatibleDC(screen)
    info = BITMAPINFO()
    info.bmiHeader.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    info.bmiHeader.biWidth = 800
    info.bmiHeader.biHeight = -600
    info.bmiHeader.biPlanes = 1
    info.bmiHeader.biBitCount = 32
    bits = ctypes.c_void_p()
    bmp = GDI32.CreateDIBSection(screen, ctypes.byref(info), 0,
                                 ctypes.byref(bits), None, 0)
    GDI32.SelectObject(dc, bmp)

    base = priv()
    print("start priv={0:.0f} KB".format(base / 1024), flush=True)

    for r in range(1, args.rounds + 1):
        for i in range(args.perround):
            if args.clip:
                GDI32.IntersectClipRect(dc, 0, 0, 800, 600)
            brush = GDI32.CreateSolidBrush(0x00FFFFFF)
            rect = RECT(i * 20, 0, i * 20 + 60, 60)
            USER32.FillRect(dc, ctypes.byref(rect), brush)
            GDI32.DeleteObject(brush)
            if args.clip:
                GDI32.SelectClipRgn(dc, None)
        if r % 50 == 0 or r == 1:
            now = priv()
            print("round {0:5d} priv={1:8.0f} KB  d={2:+9.0f} KB  gdi={3}".format(
                r, now / 1024, (now - base) / 1024,
                USER32.GetGuiResources(K32.GetCurrentProcess(), 0)), flush=True)


if __name__ == "__main__":
    main()
