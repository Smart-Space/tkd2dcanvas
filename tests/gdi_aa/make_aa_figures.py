"""make_aa_figures.py — 生成 README 里三个后端的抗锯齿对比图。

输出（默认写到 ``docs/images/``）：
    aa-native.png   原生 Tk 画布（GDI，无抗锯齿）
    aa-gdi.png      本仓库的 GDI 后端（手动超采样抗锯齿）
    aa-d2d.png      本仓库的 Direct2D 后端（PER_PRIMITIVE）

三张图是**同一个场景**、同样的尺寸、同样的位置，唯一的差别是"用什么画"。
每张图由两部分组成：

    ┌──────────────────────────────┐
    │  完整场景（1:1）              │
    ├───────────┬──────────────────┤
    │ 斜边放大   │  圆弧放大         │   <- 同一块区域，三个后端完全一致
    └───────────┴──────────────────┘

下面的两块放大图是重点：原生 Tk 画布是硬邦邦的阶梯，另外两个是连续的
灰度过渡 —— 这就是抗锯齿。

用法::

    python tests/gdi_aa/make_aa_figures.py
    python tests/gdi_aa/make_aa_figures.py --out-dir docs/images --zoom 4
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

from capture_window import print_window  # noqa: E402

W, H = 400, 240

# 放大的两块区域（逻辑像素）——三个后端必须用完全相同的区域。
# 取的区域要小：README 里图会被缩到三分之一，放大倍数得扛得住缩放。
#   斜线：45 度（y == x），阶梯最明显
#   圆弧：圆心 240,80、半径 50 的左上 1/4 圆弧
ZOOM_LINE = (76, 76, 108, 108)
ZOOM_CURVE = (189, 29, 221, 61)


# ---------------------------------------------------------------------------
# 场景：三个后端画的是完全一样的东西
# ---------------------------------------------------------------------------
def scene(canvas) -> None:
    """画一组最能体现抗锯齿的图元。"""
    # 45 度斜线：GDI 下是明显的阶梯，抗锯齿后是连续的灰度过渡
    canvas.create_line(30, 30, 150, 150, fill="#1a4a8a", width=4)

    # 圆：曲线的锯齿 vs 平滑
    canvas.create_oval(190, 30, 290, 130, fill="#e08040", outline="#703010", width=2)

    # 斜边多边形：直线段的锯齿
    canvas.create_polygon(30, 180, 100, 165, 140, 210, 60, 230,
                          fill="#50a060", outline="#204020", width=2)

    # 圆角（多边形 + round join）
    canvas.create_polygon(180, 185, 250, 170, 290, 215, 200, 235,
                          fill="", outline="#7040a0", width=8,
                          joinstyle="round")

    # 1px 细线：抗锯齿让细线不再忽粗忽细
    for i in range(3):
        canvas.create_line(310, 55 + i * 22, 390, 55 + i * 22,
                           fill="#404040", width=1)


def make_canvas(backend: str):
    """按后端返回 (root, canvas)。"""
    root = tkinter.Tk()
    # 无边框窗口：抓到的画面就是画布本身，不含标题栏，也就不会有
    # "按逻辑坐标裁剪、按物理像素截图"的错位。
    root.overrideredirect(True)
    root.geometry(f"{W}x{H}+40+40")
    root.update_idletasks()
    for _ in range(50):
        root.update()
        if root.winfo_width() > 100:
            break
        time.sleep(0.05)

    if backend == "native":
        canvas = tkinter.Canvas(root, width=W, height=H, background="white",
                                highlightthickness=0, bd=0)
    else:
        from tkd2dcanvas import Canvas

        kw = {} if backend == "d2d" else {"renderer": "gdi"}
        canvas = Canvas(root, width=W, height=H, background="white",
                        highlightthickness=0, bd=0, **kw)

    canvas.pack(fill="both", expand=True)
    scene(canvas)
    root.update()
    time.sleep(0.35)
    root.update()
    return root, canvas


def capture_scene(backend: str):
    """抓取画布内容（物理像素）。"""
    root, canvas = make_canvas(backend)
    frame = int(root.wm_frame(), 16)
    img = print_window(frame)
    root.destroy()
    if img is None:
        raise RuntimeError(f"PrintWindow failed for {backend}")
    return img


def build_figure(scene_img, zoom: int):
    """把场景和两块放大区域拼成一张图。

    放大条从场景中间隔开、左右并排，各占一半宽度 —— 这样 README 里
    缩到三分之一时，放大后的一个源像素仍然有好几个屏幕像素，阶梯看得清。
    """
    from PIL import Image, ImageDraw

    # 画布可能被 DPI 放大过，先缩回逻辑尺寸，裁剪区域才对得上。
    if scene_img.size != (W, H):
        scene_img = scene_img.resize((W, H), Image.LANCZOS)

    def zoomed(box):
        piece = scene_img.crop(box)
        if zoom > 1:
            piece = piece.resize(
                ((box[2] - box[0]) * zoom, (box[3] - box[1]) * zoom), Image.NEAREST)
        return piece

    line = zoomed(ZOOM_LINE)
    curve = zoomed(ZOOM_CURVE)

    strip_h = max(line.size[1], curve.size[1])
    gap = 6
    out = Image.new("RGB", (W, H + gap + strip_h), "white")
    out.paste(scene_img, (0, 0))
    draw = ImageDraw.Draw(out)
    draw.rectangle([0, H, W - 1, H + gap - 1], fill="#d0d0d0")
    out.paste(line, (0, H + gap))
    out.paste(curve, (W - curve.size[0], H + gap))
    return out


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out-dir", default=str(REPO / "docs" / "images"))
    parser.add_argument("--zoom", type=int, default=4,
                        help="放大倍数（1 = 原尺寸）")
    args = parser.parse_args()

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    print("生成抗锯齿对比图：")
    for backend, name in (("native", "aa-native"), ("gdi", "aa-gdi"),
                          ("d2d", "aa-d2d")):
        img = build_figure(capture_scene(backend), args.zoom)
        path = out_dir / f"{name}.png"
        img.save(path)
        print(f"  {backend:7s} -> {path} ({img.size})")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
