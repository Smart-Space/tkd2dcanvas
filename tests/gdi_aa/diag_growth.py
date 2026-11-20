"""diag_growth.py — 来回滚动时，涨的到底是哪一类内存。

bench_tinui_backforth.py 只看进程的私有提交（PrivateUsage），看不出涨的是
C++ 侧的堆、gdi32 的句柄、还是内核那边的分页。这一版把几类分开报：

* priv  —— 进程私有提交（总账）
* heap  —— 进程里所有 CRT 堆已分配的字节（C++ 侧：new/malloc、std::vector）
* gdi   —— GDI 对象数（画笔、画刷、位图、DC……）
* user  —— USER 对象数（窗口、菜单……）
* hnd   —— 进程句柄总数（文件、事件、内核对象）

用法：
    python tests/gdi_aa/diag_growth.py --backend gdi --cycles 30
    python tests/gdi_aa/diag_growth.py --backend native --cycles 30
"""

from __future__ import annotations

import argparse
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
USER32 = ctypes.WinDLL("user32", use_last_error=True)
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

K32.GetProcessHandleCount.argtypes = [wt.HANDLE, ctypes.POINTER(wt.DWORD)]
K32.GetProcessHandleCount.restype = wt.BOOL
USER32.GetGuiResources.argtypes = [wt.HANDLE, wt.DWORD]
USER32.GetGuiResources.restype = wt.DWORD

GR_GDIOBJECTS = 0
GR_USEROBJECTS = 1


def mem():
    c = C()
    c.cb = ctypes.sizeof(c)
    PSAPI.GetProcessMemoryInfo(K32.GetCurrentProcess(), ctypes.byref(c), c.cb)
    gdi = USER32.GetGuiResources(K32.GetCurrentProcess(), GR_GDIOBJECTS)
    user = USER32.GetGuiResources(K32.GetCurrentProcess(), GR_USEROBJECTS)
    hnds = wt.DWORD(0)
    K32.GetProcessHandleCount(K32.GetCurrentProcess(), ctypes.byref(hnds))
    return {
        "priv": c.PrivateUsage,
        "ws": c.WorkingSetSize,
        "gdi": gdi,
        "user": user,
        "hnd": hnds.value,
    }


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


def find_widgets(widget, out):
    """所有控件（不只是画布）。"""
    for child in widget.winfo_children():
        out.append(child)
        try:
            find_widgets(child, out)
        except tkinter.TclError:
            pass
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--backend", default="gdi")
    ap.add_argument("--cycles", type=int, default=30)
    # 下面三个开关分别打开 bench_tinui_backforth.py 的三样"测量动作"，
    # 用来定位到底是哪一样让内存涨。
    ap.add_argument("--benchlike", action="store_true")
    ap.add_argument("--bind", action="store_true",
                    help="给所有控件绑 <Configure>（一次性）")
    ap.add_argument("--cmds", action="store_true",
                    help="每趟问一次 Tcl 有多少条命令")
    ap.add_argument("--find", action="store_true",
                    help="每趟把 804 个图元取成 Python 对象")
    ap.add_argument("--sleep", type=float, default=1.0)
    args = ap.parse_args()

    sys.path.insert(0, str(HERE))
    from run_tinui import canvas_class  # noqa: E402

    import tkinter
    tkinter.Canvas = canvas_class(args.backend)

    def measure():
        import tkinter
        root = tkinter._default_root
        canvases = find_canvases(root, [])
        scrollable = []
        for cv in canvases:
            try:
                region = str(cv.cget("scrollregion"))
            except tkinter.TclError:
                continue
            parts = region.split()
            if len(parts) == 4:
                try:
                    h = float(parts[3])
                except ValueError:
                    continue
                if h > cv.winfo_height() + 4:
                    scrollable.append(cv)

        print(f"[{args.backend}] 可滚动画布 {len(scrollable)} 个", flush=True)

        def show(tag, m):
            print(f"[{args.backend}] {tag:>10}  priv={m['priv']/1024:8.0f} KB "
                  f"ws={m['ws']/1024:8.0f} KB "
                  f"gdi={m['gdi']:5d} user={m['user']:4d} hnd={m['hnd']:5d}",
                  flush=True)

        show("start", mem())
        time.sleep(args.sleep)
        root.update()
        show("idle", mem())

        if args.benchlike or args.bind:
            # 和 bench_tinui_backforth.py 一样：每趟都把 804 个图元取出来，
            # 再问一次 Tcl 有多少条命令。
            for cv in find_widgets(root, []):
                try:
                    cv.bind("<Configure>", lambda e: None, True)
                except tkinter.TclError:
                    pass

        base = None
        for cycle in range(1, args.cycles + 1):
            for frac in (0.3, 0.0):
                for cv in scrollable:
                    try:
                        cv.yview_moveto(frac)
                    except tkinter.TclError:
                        pass
                root.update()
            root.update()
            if args.benchlike or args.cmds:
                ncmd = len(root.tk.call("info", "commands"))
            if args.benchlike or args.find:
                nitem = sum(len(cv.find_all()) for cv in canvases)
            m = mem()
            if base is None:
                base = m
            d_priv = (m["priv"] - base["priv"]) / 1024
            print(f"[{args.backend}] cycle {cycle:3d}  "
                  f"priv={m['priv']/1024:8.0f} KB ws={m['ws']/1024:8.0f} KB "
                  f"dpriv={d_priv:+8.0f} KB "
                  f"dgdi={m['gdi']-base['gdi']:+5d} duser={m['user']-base['user']:+4d} "
                  f"dhnd={m['hnd']-base['hnd']:+5d}", flush=True)
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
