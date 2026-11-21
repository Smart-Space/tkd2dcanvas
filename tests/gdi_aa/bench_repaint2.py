"""bench_repaint2.py 鈥?鐪熸瑙﹀彂閲嶇粯鐨勮€楁椂瀵规瘮銆?

`bench_repaint.py` 閲岀殑"鏁村睆閲嶇粯"鍏跺疄娌℃湁绛夊埌 WM_PAINT 琚鐞嗭紝娴嬪嚭鏉ョ殑鏄?
configure 鏈韩鐨勮€楁椂銆傝繖閲屽湪 configure 涔嬪悗澶氳窇涓€娆?update()锛屽苟涓斿杞彇鏈€灏忓€硷紝
纭繚娴嬪埌鐨勬槸鐪熸鐨勭粯鍒躲€?

鍦烘櫙鍙€夛細
    --shapes N   N 涓?24x24 鐨勫皬妞渾锛堥粯璁?200锛?
    --big        棰濆鍔犱竴涓崰婊＄敾甯冪殑澶фき鍦嗭紙鍒嗗潡绛栫暐瀵瑰畠褰卞搷鏈€澶э級
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
    opts = {} if backend == "d2d" else {"renderer": backend}
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
        # 鎹釜鑳屾櫙鑹诧紝寮哄埗鏁村潡閲嶇敾锛沜onfigure + update 鐨勬椂闂村氨鏄粯鍒舵椂闂?
        # 锛圛nvalidateRect 瑙﹀彂鐨?WM_PAINT 灏卞湪杩欐 update 閲岃澶勭悊锛夈€?
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
    ap.add_argument("--backend", default="",
                    help="only measure this back end (d2d, d2d-software, gdi)")
    args = ap.parse_args()
    backends = (args.backend,) if args.backend else ("d2d", "gdi")
    for backend in backends:
        root, c = build(backend, args.shapes, args.big)
        full = measure(root, c, args.rounds)
        print(f"  {backend:12s} full repaint {full*1000:7.2f} ms "
              f"({args.shapes} shapes{' + big' if args.big else ''})")
        root.destroy()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
