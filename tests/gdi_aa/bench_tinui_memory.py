"""bench_tinui_memory.py — 在 TinUI 演示里量两个后端的内存占用。

TinUI 的界面里有可滚动的区域（PageView / 列表）。这里在演示启动后，
把所有带滚动区域的画布滚一遍，每滚一步量一次进程内存。

用法：
    python tests/gdi_aa/bench_tinui_memory.py gdi
    python tests/gdi_aa/bench_tinui_memory.py native
"""

from __future__ import annotations

import ctypes
import ctypes.wintypes as wt
import os
import runpy
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
HERE = REPO / "tests" / "tinui"
sys.path.insert(0, str(REPO / "python"))

try:
    ctypes.windll.shcore.SetProcessDpiAwareness(2)
except Exception:  # noqa: BLE001
    pass

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


def find_canvases(widget, out):
    import tkinter
    for child in widget.winfo_children():
        if isinstance(child, tkinter.Canvas):
            out.append(child)
        try:
            find_canvases(child, out)
        except tkinter.TclError:
            pass
    return out


def main():
    backend = sys.argv[1] if len(sys.argv) > 1 else "gdi"
    sys.path.insert(0, str(HERE))
    sys.path.insert(0, str(REPO / "tests" / "tinui"))
    from run_tinui import canvas_class  # noqa: E402

    import tkinter
    tkinter.Canvas = canvas_class(backend)

    def measure():
        import tkinter
        root = tkinter._default_root
        canvases = find_canvases(root, [])
        scrollable = []
        for cv in canvases:
            try:
                region = cv.cget("scrollregion")
            except tkinter.TclError:
                continue
            if region and len(str(region).split()) == 4:
                try:
                    w = float(str(region).split()[2])
                    h = float(str(region).split()[3])
                except ValueError:
                    continue
                if h > cv.winfo_height() + 4:
                    scrollable.append((cv, w, h))
        print(f"[{backend}] 画布 {len(canvases)} 个，可滚动的 {len(scrollable)} 个", flush=True)
        ws, pr = mem()
        print(f"[{backend}] start     ws={ws/1024:8.0f} KB priv={pr/1024:8.0f} KB", flush=True)
        for step in range(12):
            for cv, w, h in scrollable:
                try:
                    cv.yview_scroll(3, "units")
                except tkinter.TclError:
                    pass
            root.update()
            time.sleep(0.08)
            root.update()
            ws, pr = mem()
            print(f"[{backend}] scroll {step:2d}  ws={ws/1024:8.0f} KB priv={pr/1024:8.0f} KB", flush=True)
        root.destroy()

    import tkinter
    orig_init = tkinter.Tk.__init__

    def __init__(self, *a, **kw):
        orig_init(self, *a, **kw)
        self.after(2500, measure)

    tkinter.Tk.__init__ = __init__

    cwd = os.getcwd()
    os.chdir(HERE)
    try:
        runpy.run_path(str(HERE / "TinUI.py"), run_name="__main__")
    finally:
        os.chdir(cwd)


if __name__ == "__main__":
    main()
