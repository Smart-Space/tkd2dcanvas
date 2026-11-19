"""compare_scrolled.py — 滚动后对比 GDI 与 Direct2D 两个后端的渲染结果。

用法::

    python tests/gdi_aa/compare_scrolled.py [--out 对比图.png]

两个后端画同样的可滚动场景，滚动到相同位置后截图，逐像素比较并输出并排图。
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
SCROLL_TO = 260


def scene(c: Canvas) -> None:
    for i in range(60):
        y = i * 20
        c.create_rectangle(10, y, 160, y + 16, fill="#d8e4f0", outline="#406080")
        c.create_text(170, y + 8, text=f"row {i}", fill="#202020", anchor="w")
        c.create_oval(300, y, 340, y + 16, fill="#e09090", outline="#803030")
        c.create_line(350, y, 410, y + 16, fill="#204080", width=2)


def render(backend: str, path: Path) -> None:
    root = tkinter.Tk()
    root.title(f"scrolled {backend}")
    root.geometry(f"{WIDTH}x{HEIGHT}+40+40")
    options = {"renderer": backend} if backend == "gdi" else {}
    canvas = Canvas(root, width=WIDTH, height=HEIGHT, background="white", **options)
    canvas.pack(fill="both", expand=True)
    scene(canvas)
    canvas.configure(scrollregion=(0, 0, WIDTH, 60 * 20))
    root.update()
    canvas.yview_moveto(SCROLL_TO / (60 * 20))
    root.update()
    time.sleep(0.3)
    root.update()
    print(f"{backend}: yview={canvas.yview()} winfo={canvas.winfo_width()}x"
          f"{canvas.winfo_height()}")
    image = print_window(int(root.wm_frame(), 16))
    if image is None:
        raise RuntimeError("PrintWindow failed")
    image.save(path)
    root.destroy()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out",
                        default=str(Path(__file__).with_name("compare_scrolled.png")))
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
    print(f"已保存 {out} (左: GDI 手动抗锯齿, 右: Direct2D)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
