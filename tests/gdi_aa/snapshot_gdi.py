"""snapshot_gdi.py — 抓取 GDI 后端画布的原始像素，用来做改动前后的逐像素对比。

用法::

    python tests/gdi_aa/snapshot_gdi.py --out before.png
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
from capture_window import print_window, window_rect  # noqa: E402

W, H = 420, 300


def scene(c: Canvas) -> None:
    c.create_rectangle(10, 10, 120, 80, fill="#e0e0e0", outline="#404040", width=2)
    c.create_oval(140, 10, 240, 80, fill="#e09090", outline="#803030", width=2)
    c.create_line(260, 10, 400, 80, fill="#204080", width=3)
    c.create_polygon(10, 100, 120, 100, 120, 180, 10, 180, fill="#90c0f0",
                      outline="#103060")
    c.create_arc(140, 100, 240, 180, start=30, extent=140, style="pieslice",
                 fill="#ffe080", outline="#806000", width=2)
    c.create_text(20, 200, text="Hello canvas", fill="#202020")
    c.create_rectangle(200, 200, 300, 250, fill="#b0d0b0", outline="#406050")
    # 整数坐标的矩形（快路径的候选）
    c.create_rectangle(310, 200, 410, 250, fill="#d0b0b0", outline="#604040")
    # 半像素坐标的矩形（必须走抗锯齿）
    c.create_rectangle(310.5, 260, 410.5, 290, fill="#b0d0d0", outline="#406040")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    root = tkinter.Tk()
    root.geometry(f"{W}x{H}+40+40")
    root.update_idletasks()
    for _ in range(50):
        root.update()
        if root.winfo_width() > 100:
            break
        time.sleep(0.05)

    c = Canvas(root, width=W, height=H, background="white", renderer="gdi")
    c.pack(fill="both", expand=True)
    scene(c)
    root.update()
    time.sleep(0.4)
    root.update()

    frame = int(root.wm_frame(), 16)
    fl, ft, _, _ = window_rect(frame)
    ox = c.winfo_rootx() - fl
    oy = c.winfo_rooty() - ft
    cw, ch = c.winfo_width(), c.winfo_height()
    img = print_window(frame).crop((ox, oy, ox + cw, oy + ch))
    img.save(args.out)
    print(f"saved {args.out} ({img.size})")
    root.destroy()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
