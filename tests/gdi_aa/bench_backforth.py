"""bench_backforth.py — 同一片区域来回滚动时，内存是不是一直涨。

场景：把画布滚到 y=1000，再滚回 y=0，来回 30 趟。每趟量一次进程内存。
如果内存一直涨，就是有东西在泄漏；如果涨到某个值就不动了，那是
"缓冲只增不减"造成的（一个已知的、但应该被修掉的行为）。
"""

from __future__ import annotations

import argparse
import ctypes
import ctypes.wintypes as wt
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "python"))

try:
    ctypes.windll.shcore.SetProcessDpiAwareness(2)
except Exception:  # noqa: BLE001
    pass

import tkinter  # noqa: E402
from tkd2dcanvas import Canvas  # noqa: E402

K32 = ctypes.WinDLL("kernel32", use_last_error=True)
PSAPI = ctypes.WinDLL("psapi", use_last_error=True)
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


def mem():
    c = C()
    c.cb = ctypes.sizeof(c)
    PSAPI.GetProcessMemoryInfo(K32.GetCurrentProcess(), ctypes.byref(c), c.cb)
    return c.WorkingSetSize, c.PrivateUsage


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--backend", default="gdi")
    ap.add_argument("--cycles", type=int, default=20)
    args = ap.parse_args()

    root = tkinter.Tk()
    root.geometry("600x400+40+40")
    root.update_idletasks()
    for _ in range(60):
        root.update()
        if root.winfo_width() > 100:
            break
        time.sleep(0.05)

    opts = {"renderer": args.backend} if args.backend in ("gdi", "d2d") else {}
    c = Canvas(root, width=600, height=400, background="white",
               scrollregion=(0, 0, 600, 6000), **opts)
    c.pack(fill="both", expand=True)
    for i in range(150):
        y = 10 + i * 20
        c.create_oval(10, y, 34, y + 24, fill="#e09090", outline="#803030", width=2)
    root.update()
    time.sleep(0.4)
    root.update()

    ws, pr = mem()
    print(f"[{args.backend}] start      ws={ws/1024:8.0f} KB priv={pr/1024:8.0f} KB", flush=True)
    for cycle in range(1, args.cycles + 1):
        for frac in (0.15, 0.0):     # 滚到 y=900，再滚回来
            c.yview_moveto(frac)
            root.update()
        root.update()
        ws, pr = mem()
        print(f"[{args.backend}] cycle {cycle:3d} ws={ws/1024:8.0f} KB priv={pr/1024:8.0f} KB",
              flush=True)
    root.destroy()


if __name__ == "__main__":
    main()
