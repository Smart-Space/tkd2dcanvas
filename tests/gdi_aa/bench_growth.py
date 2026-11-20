"""bench_growth.py — 定位"滚动时内存上升"的来源。

场景：画布 600x400，可滚动内容高 6000。内容里放一个**大图元**（600x400 的椭圆），
位于 y=3000 —— 只有滚动到那里它才会进入视口。

抗锯齿引擎的离屏暂存缓冲是按**图元本身的大小**分配的（不是按可见条带），
所以这个大图元一进入视口，暂存缓冲就会涨到 600x400x16x4 = 15.4 MB。
"""

from __future__ import annotations

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
    backend = sys.argv[1] if len(sys.argv) > 1 else "gdi"
    root = tkinter.Tk()
    root.geometry("600x400+40+40")
    root.update_idletasks()
    for _ in range(60):
        root.update()
        if root.winfo_width() > 100:
            break
        time.sleep(0.05)

    opts = {"renderer": backend} if backend == "gdi" else {}
    c = Canvas(root, width=600, height=400, background="white",
               scrollregion=(0, 0, 600, 6000), **opts)
    c.pack(fill="both", expand=True)

    # 一堆小图元 + 一个位于 y=3000 的大图元
    for i in range(150):
        y = 10 + i * 20
        c.create_oval(10, y, 34, y + 24, fill="#e09090", outline="#803030", width=2)
    c.create_oval(0, 3000, 600, 3400, fill="#88bb88", outline="#204020", width=3)

    root.update()
    time.sleep(0.5)
    root.update()
    ws, pr = mem()
    print(f"initial paint     ws={ws/1024:8.0f} KB  priv={pr/1024:8.0f} KB", flush=True)

    # 逐步滚动到大图元
    for frac in (0.1, 0.2, 0.3, 0.4, 0.45, 0.5, 0.55, 0.6):
        c.yview_moveto(frac)
        root.update()
        time.sleep(0.1)
        root.update()
        ws, pr = mem()
        print(f"yview {frac:4.2f}       ws={ws/1024:8.0f} KB  priv={pr/1024:8.0f} KB", flush=True)

    # 再滚回去
    c.yview_moveto(0.0)
    root.update()
    time.sleep(0.1)
    root.update()
    ws, pr = mem()
    print(f"back to top       ws={ws/1024:8.0f} KB  priv={pr/1024:8.0f} KB", flush=True)
    root.destroy()


if __name__ == "__main__":
    main()
