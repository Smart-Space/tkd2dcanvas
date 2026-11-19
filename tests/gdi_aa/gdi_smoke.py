"""gdi_smoke.py — 冒烟测试：新建一个使用 GDI 渲染器的画布控件。

用法::

    python tests/gdi_aa/gdi_smoke.py [--backend gdi|d2d] [--out 截图.png]

会画几个形状，然后截图保存（用 PrintWindow 抓窗口内容，和窗口是否被遮挡
无关）。
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

try:  # 让窗口按物理像素渲染，否则截图会缩放错位
    ctypes.windll.shcore.SetProcessDpiAwareness(2)
except Exception:  # noqa: BLE001 - 旧系统上可能没有这个 API
    pass

import tkinter  # noqa: E402

from tkd2dcanvas import Canvas  # noqa: E402

from capture_window import print_window  # noqa: E402


def draw(canvas: Canvas) -> None:
    canvas.create_oval(20, 20, 140, 140, fill="#e09090", outline="#803030", width=2)
    canvas.create_line(170, 30, 300, 130, fill="#204080", width=3)
    canvas.create_polygon(330, 20, 420, 40, 400, 140, 320, 110,
                          fill="#90c0f0", outline="#103060")
    canvas.create_arc(20, 170, 140, 260, start=30, extent=140, style="pieslice",
                      fill="#ffe080", outline="#806000", width=2)
    canvas.create_rectangle(170, 170, 290, 250, fill="#b0d0b0", outline="")
    canvas.create_text(350, 210, text="GDI text", fill="#404040")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--backend", default="gdi", choices=["gdi", "d2d"])
    parser.add_argument("--out", default=str(Path(__file__).with_name("gdi_smoke.png")))
    args = parser.parse_args()

    root = tkinter.Tk()
    root.title(f"tkd2dcanvas ({args.backend} renderer)")
    root.geometry("480x300+120+120")

    options = {}
    if args.backend == "gdi":
        options["renderer"] = "gdi"
    canvas = Canvas(root, width=460, height=280, background="white", **options)
    canvas.pack(fill="both", expand=True)
    draw(canvas)
    root.update()
    time.sleep(0.4)
    root.update()

    hwnd = int(root.wm_frame(), 16)
    image = print_window(hwnd)
    if image is None:
        print("PrintWindow failed")
        return 1
    image.save(args.out)
    print(f"saved {args.out} ({image.width}x{image.height})")
    root.destroy()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
