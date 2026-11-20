"""bench_repaint2.py — 真正触发重绘的耗时对比。

`bench_repaint.py` 里的"整屏重绘"其实没有等到 WM_PAINT 被处理，测出来的是
configure 本身的耗时。这里在 configure 之后多跑一次 update()，并且多轮取最小值，
确保测到的是真正的绘制。

场景可选：
    --shapes N   N 个 24x24 的小椭圆（默认 200）
    --big        额外加一个占满画布的大椭圆（分块策略对它影响最大）
"""

from __future__ import annotations

import argparse
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
from tkd2dcanvas import Canvas  # noqa: E402

W, H = 600, 400


def build(backend: str, shapes: int, big: bool):
    root = tkinter.Tk()
    root.geometry(f"{W}x{H}+40+40")
    root.update_idletasks()
    for _ in range(60):
        root.update()
        if root.winfo_width() > 100:
            break
        time.sleep(0.05)
    opts = {"renderer": backend} if backend == "gdi" else {}
    c = Canvas(root, width=W, height=H, background="white", **opts)
    c.pack(fill="both", expand=True)
    for i in range(shapes):
        x = 10 + (i % 20) * 28
        y = 10 + (i // 20) * 28
        c.create_oval(x, y, x + 24, y + 24, fill="#e09090", outline="#803030", width=2)
    if big:
        c.create_oval(0, 0, W, H, fill="#88bb88", outline="#204020", width=3)
    root.update()
    time.sleep(0.4)
    root.update()
    return root, c


def measure(root, c, rounds: int) -> float:
    best = None
    for i in range(rounds):
        # 换个背景色，强制整块重画；configure + update 的时间就是绘制时间
        # （InvalidateRect 触发的 WM_PAINT 就在这次 update 里被处理）。
        t0 = time.perf_counter()
        c.configure(background="#ffffff" if i % 2 else "#fefefe")
        root.update()
        root.update()
        dt = time.perf_counter() - t0
        best = dt if best is None else min(best, dt)
    return best


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--shapes", type=int, default=200)
    ap.add_argument("--rounds", type=int, default=15)
    ap.add_argument("--big", action="store_true")
    args = ap.parse_args()
    for backend in ("d2d", "gdi"):
        root, c = build(backend, args.shapes, args.big)
        full = measure(root, c, args.rounds)
        print(f"  {backend:4s} full repaint {full*1000:7.2f} ms "
              f"({args.shapes} shapes{' + big' if args.big else ''})")
        root.destroy()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
