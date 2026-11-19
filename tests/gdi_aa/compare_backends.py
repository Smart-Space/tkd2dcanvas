"""compare_backends.py — 对比 GDI（手动抗锯齿）和 Direct2D 两个后端的渲染结果。

用法::

    python tests/gdi_aa/compare_backends.py [--out 对比图.png]

两个后端画同样的场景，然后逐像素比较，报告差异的位置，并输出并排对比图。
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

try:  # 让窗口按物理像素渲染
    ctypes.windll.shcore.SetProcessDpiAwareness(2)
except Exception:  # noqa: BLE001
    pass

import tkinter  # noqa: E402

from tkd2dcanvas import Canvas  # noqa: E402

from capture_window import print_window  # noqa: E402

WIDTH, HEIGHT = 420, 300


def scene(c: Canvas) -> None:
    c.create_rectangle(10, 10, 120, 80, fill="#e0e0e0", outline="#404040", width=2)
    c.create_oval(140, 10, 240, 80, fill="#e09090", outline="#803030", width=2)
    c.create_line(260, 10, 400, 80, fill="#204080", width=3)
    c.create_polygon(10, 100, 120, 100, 120, 180, 10, 180, fill="#90c0f0",
                      outline="#103060")
    c.create_arc(140, 100, 240, 180, start=30, extent=140, style="pieslice",
                 fill="#ffe080", outline="#806000", width=2)
    c.create_text(20, 200, text="Hello 画布", fill="#202020", font="TkDefaultFont")
    c.create_text(20, 230, text="second line", fill="#602020", font="TkDefaultFont")
    c.create_rectangle(200, 200, 300, 250, fill="#b0d0b0", outline="#406050")


def render(backend: str, path: Path) -> None:
    root = tkinter.Tk()
    root.title(f"compare {backend}")
    root.geometry(f"{WIDTH}x{HEIGHT}+40+40")
    options = {"renderer": backend} if backend == "gdi" else {}
    canvas = Canvas(root, width=WIDTH, height=HEIGHT, background="white", **options)
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
    parser.add_argument("--out", default=str(Path(__file__).with_name("compare_backends.png")))
    args = parser.parse_args()

    out = Path(args.out)
    gdi_path = out.with_name(out.stem + "_gdi.png")
    d2d_path = out.with_name(out.stem + "_d2d.png")

    render("gdi", gdi_path)
    render("d2d", d2d_path)

    from PIL import Image, ImageChops

    gdi = Image.open(gdi_path).convert("RGB")
    d2d = Image.open(d2d_path).convert("RGB")
    if gdi.size != d2d.size:
        print(f"尺寸不同: gdi={gdi.size} d2d={d2d.size}")

    diff = ImageChops.difference(gdi, d2d)
    bbox = diff.getbbox()
    gray = diff.convert("L")
    histogram = gray.histogram()
    total = gray.size[0] * gray.size[1]
    different = sum(histogram[1:])
    big = sum(histogram[32:])
    print(f"差异像素: {different}/{total} ({100.0 * different / total:.1f}%), "
          f"明显差异(>32): {big} ({100.0 * big / total:.1f}%)")
    print(f"差异包围盒: {bbox}")

    # 并排对比图
    scale = 2
    w, h = gdi.size
    canvas = Image.new("RGB", (w * 2 * scale + 20, h * scale), "white")
    canvas.paste(gdi.resize((w * scale, h * scale), Image.NEAREST), (0, 0))
    canvas.paste(d2d.resize((w * scale, h * scale), Image.NEAREST),
                  (w * scale + 20, 0))
    canvas.save(out)
    print(f"已保存 {out} (左: GDI 手动抗锯齿, 右: Direct2D)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
