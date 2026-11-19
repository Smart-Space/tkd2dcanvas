"""compare_round_caps.py — 对比 GDI 手动抗锯齿 与 Direct2D 在圆头线段上的渲染。

画几组水平线段（width 1..8，capstyle=round），分别用 d2d 和 gdi 两个后端
渲染并截图，报告两端的差异的位置。
"""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

import ctypes

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "python"))
sys.path.insert(0, str(REPO / "tools"))

try:
    ctypes.windll.shcore.SetProcessDpiAwareness(2)
except Exception:
    pass

import tkinter  # noqa: E402

from capture_window import print_window  # noqa: E402

from tkd2dcanvas import Canvas  # noqa: E402

WIDTH, HEIGHT = 320, 400


def scene(c: Canvas) -> None:
    y = 10
    for width in (1, 2, 3, 4, 5, 6, 7, 8):
        c.create_line(30, y, 290, y, fill="black", width=width,
                      capstyle="round")
        c.create_text(2, y - 6, text=str(width), anchor="w",
                      fill="#c00000", font=("Consolas", 8))
        y += 38


def render(backend: str, path: Path) -> None:
    root = tkinter.Tk()
    root.title(f"round caps {backend}")
    root.geometry(f"{WIDTH}x{HEIGHT}+40+40")
    options = {"renderer": backend} if backend == "gdi" else {}
    canvas = Canvas(root, width=WIDTH, height=HEIGHT, background="white",
                    **options)
    canvas.pack(fill="both", expand=True)
    scene(canvas)
    root.update()
    time.sleep(0.3)
    root.update()
    image = print_window(int(root.wm_frame(), 16))
    if image is None:
        raise RuntimeError("PrintWindow failed")
    image.save(path)
    root.destroy()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default=str(Path(__file__).with_name(
        "compare_round_caps.png")))
    args = parser.parse_args()

    out = Path(args.out)
    gdi_path = out.with_name(out.stem + "_gdi.png")
    d2d_path = out.with_name(out.stem + "_d2d.png")

    render("gdi", gdi_path)
    render("d2d", d2d_path)

    from PIL import Image, ImageChops

    gdi = Image.open(gdi_path).convert("RGB")
    d2d = Image.open(d2d_path).convert("RGB")
    diff = ImageChops.difference(gdi, d2d)
    gray = diff.convert("L")
    histogram = gray.histogram()
    total = gray.size[0] * gray.size[1]
    different = sum(histogram[1:])
    big = sum(histogram[32:])
    print(f"差异像素: {different}/{total} ({100.0 * different / total:.1f}%), "
          f"明显差异(>32): {big} ({100.0 * big / total:.1f}%)")

    scale = 2
    w, h = gdi.size
    canvas = Image.new("RGB", (w * 2 * scale + 20, h * scale), "white")
    canvas.paste(gdi.resize((w * scale, h * scale), Image.NEAREST), (0, 0))
    canvas.paste(d2d.resize((w * scale, h * scale), Image.NEAREST),
                 (w * scale + 20, 0))
    canvas.save(out)
    print(f"已保存 {out} (左= GDI 手动抗锯齿, 右= Direct2D)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
