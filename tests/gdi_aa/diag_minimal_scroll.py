"""diag_minimal_scroll.py -- is the growth in the canvas widget or in the app?

The TinUI demo grows while scrolling; Tk's own canvas does not.  The demo
does a lot of things around the canvas (containers, timers, bindings), so
this script strips all of that away: a plain Tk root, one canvas, a scroll
region, some items -- and the same back and forth scroll.

Usage:
    python tests/gdi_aa/diag_minimal_scroll.py --backend gdi --cycles 40
"""

from __future__ import annotations

import argparse
import ctypes
import ctypes.wintypes as wt
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "python"))

try:
    ctypes.windll.shcore.SetProcessDpiAwareness(2)
except Exception:  # noqa: BLE001
    pass

K32 = ctypes.WinDLL("kernel32", use_last_error=True)
PSAPI = ctypes.WinDLL("psapi", use_last_error=True)
USER32 = ctypes.WinDLL("user32", use_last_error=True)
K32.GetCurrentProcess.restype = wt.HANDLE
USER32.GetGuiResources.argtypes = [wt.HANDLE, wt.DWORD]
USER32.GetGuiResources.restype = wt.DWORD


def counts() -> tuple:
    """(GDI objects, USER objects, process handles)."""
    gdi = USER32.GetGuiResources(K32.GetCurrentProcess(), 0)
    user = USER32.GetGuiResources(K32.GetCurrentProcess(), 1)
    hnds = wt.DWORD(0)
    K32.GetProcessHandleCount.argtypes = [wt.HANDLE, ctypes.POINTER(wt.DWORD)]
    K32.GetProcessHandleCount(K32.GetCurrentProcess(), ctypes.byref(hnds))
    return gdi, user, hnds.value


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


def top_regions(count: int = 8) -> list:
    """The largest committed private read/write regions (base, alloc, size)."""
    out = []
    addr = 0
    mbi = MBI()
    while addr < 0x7FFFFFFF0000:
        if not K32.VirtualQueryEx(K32.GetCurrentProcess(), ctypes.c_void_p(addr),
                                  ctypes.byref(mbi), ctypes.sizeof(mbi)):
            break
        base = mbi.BaseAddress or 0
        size = mbi.RegionSize
        if (mbi.State == 0x1000 and mbi.Type == 0x20000 and
                (mbi.Protect & 0xFF) == 0x04):
            out.append((base, mbi.AllocationBase or 0, size))
        nxt = base + size
        if nxt <= addr:
            break
        addr = nxt
    out.sort(key=lambda r: -r[2])
    return out[:count]


def region_text() -> str:
    return " ".join("{0:x}/{1:x}+{2:.0f}K".format(b, a, s / 1024)
                    for b, a, s in top_regions())


def ballast(mb: int = 64):
    """Allocate a big block through the CRT, to see which heap it lands in."""
    import ctypes
    libc = ctypes.CDLL("msvcrt")
    libc.malloc.argtypes = [ctypes.c_size_t]
    libc.malloc.restype = ctypes.c_void_p
    ptr = libc.malloc(mb * 1024 * 1024)
    if ptr:
        ctypes.memset(ptr, 0x42, mb * 1024 * 1024)
    return ptr


def priv() -> int:
    c = C()
    c.cb = ctypes.sizeof(c)
    PSAPI.GetProcessMemoryInfo(K32.GetCurrentProcess(), ctypes.byref(c), c.cb)
    return c.PrivateUsage


class MBI(ctypes.Structure):
    _fields_ = [
        ("BaseAddress", ctypes.c_void_p),
        ("AllocationBase", ctypes.c_void_p),
        ("AllocationProtect", wt.DWORD),
        ("PartitionId", wt.WORD),
        ("RegionSize", ctypes.c_size_t),
        ("State", wt.DWORD),
        ("Protect", wt.DWORD),
        ("Type", wt.DWORD),
    ]


K32.VirtualQueryEx.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.POINTER(MBI),
                                ctypes.c_size_t]
K32.VirtualQueryEx.restype = ctypes.c_size_t


def buckets() -> dict:
    """Committed private memory split by kind (bytes)."""
    out = {"heap": 0, "sec": 0, "img": 0, "other": 0}
    addr = 0
    mbi = MBI()
    while addr < 0x7FFFFFFF0000:
        if not K32.VirtualQueryEx(K32.GetCurrentProcess(), ctypes.c_void_p(addr),
                                  ctypes.byref(mbi), ctypes.sizeof(mbi)):
            break
        base = mbi.BaseAddress or 0
        size = mbi.RegionSize
        if mbi.State == 0x1000:  # MEM_COMMIT
            prot = mbi.Protect & 0xFF
            if mbi.Type == 0x1000000:      # MEM_IMAGE
                out["img"] += size
            elif mbi.Type == 0x20000:      # MEM_PRIVATE
                if prot == 0x04:           # PAGE_READWRITE
                    out["heap"] += size
                elif prot == 0x08:         # PAGE_WRITECOPY
                    out["sec"] += size
                else:
                    out["other"] += size
            else:
                out["other"] += size
        nxt = base + size
        if nxt <= addr:
            break
        addr = nxt
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--backend", default="gdi")
    ap.add_argument("--cycles", type=int, default=40)
    ap.add_argument("--items", type=int, default=200)
    ap.add_argument("--shapes", default="oval",
                    help="oval|line|rect - which items to create")
    ap.add_argument("--windowitem", action="store_true",
                    help="also put an embedded window into the canvas")
    ap.add_argument("--optloop", type=int, default=0,
                    help="only loop a single option N times")
    ap.add_argument("--opt", default="scrollregion",
                    help="which option the --optloop sets")
    args = ap.parse_args()

    import tkinter

    if args.backend == "native":
        canvas_cls = tkinter.Canvas
    else:
        from tkd2dcanvas import Canvas as canvas_cls

    root = tkinter.Tk()
    root.geometry("800x600+40+40")

    # The renderer has to be given when the widget is created: the widget
    # picks its renderer when its window is created, and a later
    # "configure -renderer" would only change the setting, not the renderer.
    options = {}
    if args.backend != "native":
        options["renderer"] = args.backend
    cv = canvas_cls(root, width=800, height=600, background="white",
                    scrollregion=(0, 0, 800, 6000), **options)
    cv.pack()

    for i in range(args.items):
        x = 20 + (i % 10) * 70
        y = 40 + (i // 10) * 250
        if args.shapes == "rect":
            # Pixel aligned rectangle with a fill and no outline: the GDI
            # renderer takes the "no anti-aliasing needed" fast path, so the
            # AA engine is not involved at all.
            cv.create_rectangle(x, y, x + 60, y + 60, fill="#4080c0", outline="")
        elif args.shapes == "line":
            cv.create_line(x, y, x + 60, y + 200, fill="black", width=2)
        else:
            cv.create_oval(x, y, x + 60, y + 60, fill="#4080c0", outline="")
            cv.create_line(x, y, x + 60, y + 200, fill="black", width=2)

    if args.windowitem:
        frame = tkinter.Frame(root, width=200, height=120, bg="#c0c0c0")
        cv.create_window(100, 1000, window=frame, width=200, height=120)

    root.update()
    root.update()

    print("[{0}] start priv={1:.0f} KB".format(args.backend, priv() / 1024),
          flush=True)

    if args.optloop:
        # TinUI 的滚动容器每秒钟都会做一次 config(scrollregion=bbox)。
        base = priv()
        print("[{0}] regions at start: {1}".format(
            args.backend, region_text()), flush=True)
        for i in range(1, args.optloop + 1):
            if args.opt == "scrollregion":
                cv.configure(scrollregion=(0, 0, 800, 6000 + (i % 3)))
            elif args.opt == "xscrollincrement":
                cv.configure(xscrollincrement=1 + (i % 2))
            elif args.opt == "highlightthickness":
                cv.configure(highlightthickness=0)
            elif args.opt == "background":
                cv.configure(background="white")
            elif args.opt == "cget":
                cv.cget("scrollregion")
            root.update()
            if i % 20 == 0 or i == 1:
                p = priv()
                g, u, h = counts()
                b = buckets()
                import gc
                import sys
                print("[{0}] optloop {1:4d} priv={2:8.0f} KB  d={3:+8.0f} KB "
                      "heap={4:8.0f} sec={5:8.0f} img={6:8.0f} "
                      "gdi={7:5d} user={8:4d} hnd={9:5d} "
                      "pyblk={10:8d} gcobj={11:6d}".format(
                          args.backend, i, p / 1024, (p - base) / 1024,
                          b["heap"] / 1024, b["sec"] / 1024, b["img"] / 1024,
                          g, u, h, sys.getallocatedblocks(),
                          len(gc.get_objects())), flush=True)
        root.destroy()
        print("[{0}] regions at end:   {1}".format(
            args.backend, region_text()), flush=True)
        ballast(64)
        print("[{0}] after 64 MB ballast: {1}".format(
            args.backend, region_text()), flush=True)
        return

    base = None
    for cycle in range(1, args.cycles + 1):
        for frac in (0.3, 0.0):
            cv.yview_moveto(frac)
            root.update()
        root.update()
        p = priv()
        if base is None:
            base = p
        print("[{0}] cycle {1:3d} priv={2:8.0f} KB  d={3:+8.0f} KB".format(
            args.backend, cycle, p / 1024, (p - base) / 1024), flush=True)
    root.destroy()


if __name__ == "__main__":
    main()
