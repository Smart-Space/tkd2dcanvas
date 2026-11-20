"""bench_tinui_backforth.py — TinUI 演示里同一片区域来回滚动，内存是不是一直涨。

和 bench_tinui_memory.py 的区别：这里不是一路往下滚，而是在**同一片区域**
来回滚（滚下去再滚回来），重复很多趟。如果内存每一趟都涨，就是泄漏；
如果涨到某个值就不动了，那是缓冲"只增不减"。
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

K32.GetProcessHeaps.argtypes = [wt.DWORD, ctypes.POINTER(wt.HANDLE)]
K32.GetProcessHeaps.restype = wt.DWORD
K32.GetProcessHeap.restype = wt.HANDLE
K32.HeapSize.argtypes = [wt.HANDLE, wt.DWORD, ctypes.c_size_t]
K32.HeapSize.restype = ctypes.c_size_t


def crt_heap_bytes() -> int:
    """进程里所有 CRT 堆已分配的字节数（能看出 C++ 侧有没有漏）。"""
    try:
        count = K32.GetProcessHeaps(0, None)
        if not count:
            return 0
        buf = (wt.HANDLE * count)()
        n = K32.GetProcessHeaps(count, buf)
        total = 0
        for i in range(n):
            h = buf[i]
            if not h:
                continue
            size = K32.HeapSize(h, 0, 0)
            if size not in (0xFFFFFFFFFFFFFFFF, 0xFFFFFFFF):
                total += size
        return total
    except Exception as exc:  # noqa: BLE001
        print("crt_heap_bytes failed:", exc, flush=True)
        return 0


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


def find_widgets(widget, out):
    """所有控件（不只是画布）——Configure 事件可能落在非画布控件上。"""
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
    ap.add_argument("--cycles", type=int, default=10)
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

        # 数一数 <Configure> 事件：TinUI 的滚动容器里
        #   bind("<Configure>", lambda _: self.update__(), True)
        # 而 update__() 每跑一次就再排一条 after 定时器链 —— 每多一个
        # Configure 事件，就多一条永久循环的定时器链。
        counter = {"n": 0}
        paths = {}
        sizes = {}

        def on_configure(event):
            counter["n"] += 1
            path = str(event.widget)
            paths[path] = paths.get(path, 0) + 1
            seq = sizes.setdefault(path, [])
            try:
                parent = event.widget.master
                pgeom = "%s %dx%d" % (parent, parent.winfo_width(),
                                      parent.winfo_height())
                mgr = event.widget.winfo_manager()
            except Exception:  # noqa: BLE001
                pgeom = "?"
                mgr = "?"
            seq.append((event.width, event.height, pgeom, mgr))
            if len(seq) > 4:
                del seq[0]

        for cv in find_widgets(root, []):
            try:
                cv.bind("<Configure>", on_configure, True)
            except tkinter.TclError:
                pass

        # 追踪 Tcl 层：看看 Configure 之前跑了什么命令
        if os.environ.get("TRACE_TCL"):
            for cmd in ("pack", "place", "grid"):
                try:
                    root.tk.eval(
                        f'trace add execution {cmd} enter '
                        f'{{puts "TRACE {cmd} $args"; flush stdout}}')
                except tkinter.TclError as exc:
                    print("trace failed:", cmd, exc, flush=True)

        # 记录内嵌画布的几何：滚动时它们到底动没动？
        embedded = [cv for cv in canvases if cv is not root.children.get("", None)]
        before_geom = {}
        for cv in canvases:
            try:
                before_geom[str(cv)] = (cv.winfo_x(), cv.winfo_y(),
                                        cv.winfo_rootx(), cv.winfo_rooty())
            except tkinter.TclError:
                pass

        print(f"[{args.backend}] 可滚动画布 {len(scrollable)} 个", flush=True)
        for cv in scrollable:
            print(f"    scroll {cv} region={cv.cget('scrollregion')} "
                  f"size={cv.winfo_width()}x{cv.winfo_height()} "
                  f"items={len(cv.find_all())}", flush=True)
        ws, pr = mem()
        print(f"[{args.backend}] start      ws={ws/1024:8.0f} KB priv={pr/1024:8.0f} KB", flush=True)

        # 空闲 3 秒（完全不滚动）：Configure 事件还在不在？
        idle_before = counter["n"]
        time.sleep(3.0)
        root.update()
        print(f"[{args.backend}] idle 3 s   configure={counter['n'] - idle_before}",
              flush=True)
        for cycle in range(1, args.cycles + 1):
            before = counter["n"]
            for frac in (0.3, 0.0):        # 同一片区域，来回滚
                for cv in scrollable:
                    try:
                        cv.yview_moveto(frac)
                    except tkinter.TclError:
                        pass
                root.update()            # 一次事件循环滚一次（真实滚轮就是这样）
            root.update()
            ws, pr = mem()
            ncmd = len(root.tk.call("info", "commands"))
            nitem = sum(len(cv.find_all()) for cv in canvases)
            pos = []
            for cv in canvases:
                try:
                    p = (cv.winfo_x(), cv.winfo_y())
                except tkinter.TclError:
                    continue
                if p != (0, 0):
                    pos.append(p)
            print(f"[{args.backend}] cycle {cycle:3d} ws={ws/1024:8.0f} KB "
                  f"priv={pr/1024:8.0f} KB cfg={counter['n']-before:5d} "
                  f"cmd={ncmd} items={nitem} pos={sorted(set(pos))[:4]}", flush=True)
            if cycle == args.cycles:
                top = sorted(paths.items(), key=lambda kv: -kv[1])[:6]
                for path, n in top:
                    print(f"    configure {path}: {n} seq={sizes.get(path)}", flush=True)
                moved = []
                for cv in canvases:
                    key = str(cv)
                    try:
                        now = (cv.winfo_x(), cv.winfo_y(),
                               cv.winfo_rootx(), cv.winfo_rooty())
                    except tkinter.TclError:
                        continue
                    if key in before_geom and before_geom[key] != now:
                        moved.append((key, before_geom[key], now))
                print(f"    滚动后位置变了的画布: {len(moved)}", flush=True)
                for key, b, a in moved[:6]:
                    print(f"      {key}: {b} -> {a}", flush=True)
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
