"""diag_gdi_path_churn.py -- does the GDI path work grow the process' commit?

The anti-aliased renderer draws every shape into an off-screen buffer
(Polygon / StrokePath) and then composites it with AlphaBlend.  Those GDI
calls allocate inside gdi32, not in the module and not in the CRT heap, so
neither the allocation probe nor the CRT heap statistics see them.

This script repeats exactly those GDI calls and watches the private commit.

Usage:
    python tests/gdi_aa/diag_gdi_path_churn.py --rounds 400 --perround 100
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
MSIMG32 = ctypes.WinDLL("msimg32", use_last_error=True)
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


class POINT(ctypes.Structure):
    _fields_ = [("x", ctypes.c_long), ("y", ctypes.c_long)]


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


class BLENDFUNCTION(ctypes.Structure):
    _fields_ = [("BlendOp", ctypes.c_ubyte), ("BlendFlags", ctypes.c_ubyte),
                ("SourceConstantAlpha", ctypes.c_ubyte), ("AlphaFormat", ctypes.c_ubyte)]


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
GDI32.ExtCreatePen.restype = wt.HPEN
GDI32.SetPolyFillMode.argtypes = [wt.HDC, ctypes.c_int]
GDI32.SetBkMode.argtypes = [wt.HDC, ctypes.c_int]
GDI32.Polygon.argtypes = [wt.HDC, ctypes.POINTER(POINT), ctypes.c_int]
GDI32.BeginPath.argtypes = [wt.HDC]
GDI32.EndPath.argtypes = [wt.HDC]
GDI32.StrokePath.argtypes = [wt.HDC]
GDI32.Polyline.argtypes = [wt.HDC, ctypes.POINTER(POINT), ctypes.c_int]
MSIMG32.AlphaBlend.argtypes = [wt.HDC, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                               ctypes.c_int, wt.HDC, ctypes.c_int, ctypes.c_int,
                               ctypes.c_int, ctypes.c_int, ctypes.POINTER(BLENDFUNCTION)]
MSIMG32.AlphaBlend.restype = wt.BOOL

PS_GEOMETRIC = 0x10000
PS_ENDCAP_FLAT = 0x000
WINDING = 2
TRANSPARENT = 1
AC_SRC_OVER = 0
AC_SRC_ALPHA = 1
DIB_RGB_COLORS = 0


class LOGBRUSH(ctypes.Structure):
    _fields_ = [("lbStyle", wt.UINT), ("lbColor", wt.COLORREF),
                ("lbHatch", ctypes.POINTER(ctypes.c_ulong))]


GDI32.ExtCreatePen.argtypes = [wt.DWORD, wt.DWORD, ctypes.POINTER(LOGBRUSH),
                                wt.DWORD, ctypes.POINTER(wt.DWORD)]


def priv() -> int:
    c = C()
    c.cb = ctypes.sizeof(c)
    PSAPI.GetProcessMemoryInfo(K32.GetCurrentProcess(), ctypes.byref(c), c.cb)
    return c.PrivateUsage


def make_dib(dc, w, h):
    info = BITMAPINFO()
    info.bmiHeader.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    info.bmiHeader.biWidth = w
    info.bmiHeader.biHeight = -h
    info.bmiHeader.biPlanes = 1
    info.bmiHeader.biBitCount = 32
    info.bmiHeader.biCompression = 0
    bits = ctypes.c_void_p()
    bmp = GDI32.CreateDIBSection(dc, ctypes.byref(info), DIB_RGB_COLORS,
                                 ctypes.byref(bits), None, 0)
    return bmp, bits


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rounds", type=int, default=400)
    ap.add_argument("--perround", type=int, default=100)
    args = ap.parse_args()

    screen = USER32.GetDC(None)
    dc = GDI32.CreateCompatibleDC(screen)
    bmp, _bits = make_dib(dc, 800, 600)
    old = GDI32.SelectObject(dc, bmp)

    # The AA engine composites from an off-screen buffer into the target,
    # so the source and the destination are *different* DCs.
    src_dc = GDI32.CreateCompatibleDC(screen)
    src_bmp, _src_bits = make_dib(src_dc, 448, 484)
    GDI32.SelectObject(src_dc, src_bmp)

    brush = GDI32.CreateSolidBrush(0x00FFFFFF)
    log = LOGBRUSH()
    log.lbStyle = 0
    log.lbColor = 0x00FFFFFF
    pen = GDI32.ExtCreatePen(PS_GEOMETRIC | PS_ENDCAP_FLAT, 4, ctypes.byref(log), 0, None)

    pts = (POINT * 64)()
    import math
    for i in range(64):
        a = 2.0 * math.pi * i / 64.0
        pts[i] = POINT(int(30 + 25 * math.cos(a)), int(30 + 25 * math.sin(a)))

    blend = BLENDFUNCTION(AC_SRC_OVER, 0, 255, AC_SRC_ALPHA)

    base = priv()
    print("start priv={0:.0f} KB gdi={1}".format(base / 1024,
          USER32.GetGuiResources(K32.GetCurrentProcess(), 0)), flush=True)

    for r in range(1, args.rounds + 1):
        for i in range(args.perround):
            GDI32.SelectObject(dc, pen)
            GDI32.SetPolyFillMode(dc, WINDING)
            GDI32.Polygon(dc, pts, 64)
            GDI32.SetBkMode(dc, TRANSPARENT)
            GDI32.BeginPath(dc)
            GDI32.Polyline(dc, pts, 64)
            GDI32.EndPath(dc)
            GDI32.StrokePath(dc)
            MSIMG32.AlphaBlend(dc, 0, 0, 60, 60, src_dc, 0, 0, 60, 60,
                               ctypes.byref(blend))
        if r % 50 == 0 or r == 1:
            now = priv()
            print("round {0:5d} priv={1:8.0f} KB  d={2:+9.0f} KB  gdi={3}".format(
                r, now / 1024, (now - base) / 1024,
                USER32.GetGuiResources(K32.GetCurrentProcess(), 0)), flush=True)

    GDI32.DeleteObject(pen)
    GDI32.DeleteObject(brush)
    GDI32.SelectObject(dc, old)
    GDI32.DeleteObject(bmp)
    GDI32.DeleteDC(dc)


if __name__ == "__main__":
    main()
