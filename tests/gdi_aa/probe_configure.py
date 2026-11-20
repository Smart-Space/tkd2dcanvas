"""probe_configure.py — 仓库的画布 vs Tk 的画布：滚动时给内嵌窗口发几个 <Configure>？

同一个场景（画布里放一个内嵌 frame，滚动几次），两边各数一遍。
Tk 的行为是基准：

    start: n=1
    after scroll down: n=1        <- 滚出视口 → 取消映射，不再移动
    after scroll back: n=2        <- 回到视口 → 重新定位（一个 Configure）
    after scroll down again: n=2

（tests/gdi_aa/build/_tk_configure_probe.tcl，跑在打过补丁的 wish86t.exe 上）
"""

from __future__ import annotations

import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "python"))

try:
    import ctypes
    ctypes.windll.shcore.SetProcessDpiAwareness(2)
except Exception:  # noqa: BLE001
    pass

import tkinter  # noqa: E402


def probe(canvas_class, label: str) -> None:
    root = tkinter.Tk()
    root.geometry("500x400+40+40")
    root.update_idletasks()
    for _ in range(60):
        root.update()
        if root.winfo_width() > 100:
            break
        time.sleep(0.05)

    opts = {}
    if label == "gdi":
        opts["renderer"] = "gdi"
    c = canvas_class(root, width=400, height=300, background="white",
                     scrollregion=(0, 0, 400, 3000), **opts)
    c.pack(fill="both", expand=True)

    frame = tkinter.Frame(c, width=200, height=200, background="#c0c0c0")
    frame.place(x=10, y=10)
    count = {"n": 0}
    frame.bind("<Configure>", lambda e: count.__setitem__("n", count["n"] + 1))
    root.update()
    root.update()

    def step(text: str) -> None:
        root.update()
        root.update()
        print(f"  {label:6s} {text:26s} configure={count['n']}", flush=True)

    step("start")
    c.yview_moveto(0.2)
    step("after scroll down")
    c.yview_moveto(0.0)
    step("after scroll back")
    c.yview_moveto(0.4)
    step("after scroll down again")
    root.destroy()


def main() -> int:
    print("Tk 的画布（基准，见 _tk_configure_probe.tcl）：")
    print("  start n=1 / scroll down n=1 / scroll back n=2 / scroll down again n=2")
    probe(tkinter.Canvas, "tk")
    try:
        from tkd2dcanvas import Canvas
        probe(Canvas, "gdi")
    except Exception as exc:  # noqa: BLE001
        print("  tkd2dcanvas 不可用:", exc)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
