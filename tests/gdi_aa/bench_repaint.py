"""bench_repaint.py — 两个后端的重绘耗时对比。

GDI 后端的抗锯齿是"超采样 + 降采样 + AlphaBlend"，每个图元都要：
  * 在 N 倍分辨率的离屏缓冲上画一遍
  * 盒式滤波降采样
  * AlphaBlend 合成回目标
所以它比 D2D 慢。这里量一下具体差多少。

注意：**画布背景也是走抗锯齿引擎的**（Win32Surface::Paint 用 fillPath
填背景），而背景是一个轴对齐、整数坐标的矩形 —— 它根本没有抗锯齿边缘，
却被要求走一遍 N 倍超采样。像素对齐矩形走 FillRect 快 250 倍。
"""

from __future__ import annotations

import argparse
import ctypes
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "python"))
sys.path.insert(0, str(REPO / "tools"))

try:
    ctypes.windll.shcore.SetProcessDpiAwareness(2)
except Exception:  # noqa: BLE001
    pass

import tkinter  # noqa: E402

from tkd2dcanvas import Canvas  # noqa: E402

W, H = 600, 400


def scene(c: Canvas, n: int) -> None:
    for i in range(n):
        x = 10 + (i % 20) * 28
        y = 10 + (i // 20) * 28
        c.create_oval(x, y, x + 24, y + 24, fill="#e09090", outline="#803030", width=2)


def build(backend: str, n: int):
    root = tkinter.Tk()
    root.geometry(f"{W}x{H}+40+40")
    root.update_idletasks()
    for _ in range(50):
        root.update()
        if root.winfo_width() > 100:
            break
        time.sleep(0.05)
    opts = {"renderer": backend} if backend == "gdi" else {}
    c = Canvas(root, width=W, height=H, background="white", **opts)
    c.pack(fill="both", expand=True)
    scene(c, n)
    root.update()
    time.sleep(0.3)
    root.update()
    return root, c


def measure(root, c, kind: str, rounds: int) -> float:
    best = None
    for _ in range(rounds):
        if kind == "scroll":
            t0 = time.perf_counter()
            c.yview_scroll(1, "units")
            root.update()
        else:
            c.configure(background="#ffffff")
            t0 = time.perf_counter()
            root.update()
        dt = time.perf_counter() - t0
        best = dt if best is None else min(best, dt)
    return best


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--shapes", type=int, default=200)
    parser.add_argument("--rounds", type=int, default=20)
    args = parser.parse_args()

    print(f"{args.shapes} 个图元，{W}x{H} 画布：")
    res = {}
    for backend in ("d2d", "gdi"):
        root, c = build(backend, args.shapes)
        res[backend] = {
            "scroll": measure(root, c, "scroll", args.rounds),
            "full": measure(root, c, "full", 10),
        }
        root.destroy()

    print(f"  {'':6s}{'滚动一帧':>12s}{'整屏重绘':>12s}")
    for backend in ("d2d", "gdi"):
        r = res[backend]
        print(f"  {backend:6s}{r['scroll']*1000:9.2f} ms{r['full']*1000:9.2f} ms")
    print(f"  GDI 整屏重绘是 D2D 的 {res['gdi']['full']/res['d2d']['full']:.2f} 倍")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
