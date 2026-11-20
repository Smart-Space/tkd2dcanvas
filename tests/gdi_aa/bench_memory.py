"""bench_memory.py — 滚动过程中两个后端的内存占用对比。

抗锯齿后端（GDI）每个图元都要在 N 倍分辨率的离屏缓冲上画一遍，所以它有一块
"超采样暂存缓冲"。这块缓冲是**按最大图元**分配的：画过一次大图元之后，缓冲就
保持那么大（`GdiAntialias::EnsureScratch` 只增不减）。

滚动时新的图元进入视口，如果其中有更大的图元，暂存缓冲就会变大 —— 这正是
"滚动时内存占用上升、到一定程度稳定"的可能来源。

这里量三样东西：
  * 工作集（WorkingSetSize）
  * 私有提交（PrivateUsage）—— 真正"占住"的内存
  * GDI 对象数（GetGuiResources）—— 句柄泄漏的探针
"""

from __future__ import annotations

import argparse
import ctypes
import ctypes.wintypes as wt
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "python"))

try:
    ctypes.windll.shcore.SetProcessDpiAwareness(2)
except Exception:  # noqa: BLE001
    pass

import tkinter  # noqa: E402

from tkd2dcanvas import Canvas  # noqa: E402

W, H = 600, 400
TALL = 6000  # 画布内容高度


class PROCESS_MEMORY_COUNTERS_EX(ctypes.Structure):
    _fields_ = [
        ("cb", wt.DWORD),
        ("PageFaultCount", wt.DWORD),
        ("PeakWorkingSetSize", ctypes.c_size_t),
        ("WorkingSetSize", ctypes.c_size_t),
        ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
        ("QuotaPagedPoolUsage", ctypes.c_size_t),
        ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
        ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
        ("PagefileUsage", ctypes.c_size_t),
        ("PeakPagefileUsage", ctypes.c_size_t),
        ("PrivateUsage", ctypes.c_size_t),
    ]


class PROCESS_MEMORY_COUNTERS(ctypes.Structure):
    _fields_ = PROCESS_MEMORY_COUNTERS_EX._fields_[:9]


_KERNEL32 = ctypes.WinDLL("kernel32", use_last_error=True)
_PSAPI = ctypes.WinDLL("psapi", use_last_error=True)
_USER32 = ctypes.WinDLL("user32", use_last_error=True)

_KERNEL32.GetCurrentProcess.restype = wt.HANDLE
_PSAPI.GetProcessMemoryInfo.argtypes = [wt.HANDLE,
                                        ctypes.POINTER(PROCESS_MEMORY_COUNTERS_EX),
                                        wt.DWORD]
_PSAPI.GetProcessMemoryInfo.restype = wt.BOOL
_USER32.GetGuiResources.argtypes = [wt.HANDLE, wt.DWORD]
_USER32.GetGuiResources.restype = wt.DWORD


def _counters() -> PROCESS_MEMORY_COUNTERS_EX:
    c = PROCESS_MEMORY_COUNTERS_EX()
    c.cb = ctypes.sizeof(c)
    if not _PSAPI.GetProcessMemoryInfo(_KERNEL32.GetCurrentProcess(),
                                        ctypes.byref(c), c.cb):
        raise ctypes.WinError(ctypes.get_last_error())
    return c


def mem() -> dict[str, int]:
    c = _counters()
    return {
        "ws": c.WorkingSetSize,
        "priv": c.PrivateUsage,
        "peakws": c.PeakWorkingSetSize,
    }


def gdi_objects() -> int:
    return _USER32.GetGuiResources(0, 0)  # GR_GDIOBJECTS


def user_objects() -> int:
    return _USER32.GetGuiResources(0, 1)  # GR_USEROBJECTS


def build(backend: str, factor: int, big: bool):
    root = tkinter.Tk()
    root.geometry(f"{W}x{H}+40+40")
    root.update_idletasks()
    for _ in range(60):
        root.update()
        if root.winfo_width() > 100:
            break
        time.sleep(0.05)

    opts = {"renderer": backend} if backend == "gdi" else {}
    if backend == "gdi":
        opts["antialiasfactor"] = factor
    c = Canvas(root, width=W, height=H, background="white",
               scrollregion=(0, 0, W, TALL), **opts)
    c.pack(fill="both", expand=True)

    # 场景：每 40 像素一行小图元；越往下偶有大图元。
    for i in range(TALL // 40):
        y = 10 + i * 40
        x = 10 + (i % 5) * 30
        c.create_oval(x, y, x + 24, y + 24, fill="#e09090", outline="#803030", width=2)
        c.create_line(x + 40, y, x + 160, y + 18, fill="#204080", width=2)
        if big and i % 20 == 17:
            # 一块大图元：离屏暂存缓冲会被撑到 4 倍分辨率
            c.create_polygon(x + 200, y, x + 520, y + 10, x + 520, y + 150,
                             x + 200, y + 150, fill="#88bb88", outline="#204020")
    root.update()
    time.sleep(0.4)
    root.update()
    return root, c


def run(backend: str, factor: int, big: bool, steps: int) -> list[tuple[int, dict[str, int], int, int]]:
    root, c = build(backend, factor, big)
    samples = []
    m = mem()
    samples.append((0, m, gdi_objects(), user_objects()))
    for step in range(1, steps + 1):
        c.yview_scroll(3, "units")
        root.update()
        time.sleep(0.05)
        root.update()
        m = mem()
        samples.append((step, m, gdi_objects(), user_objects()))
    root.destroy()
    return samples


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--steps", type=int, default=30)
    ap.add_argument("--factor", type=int, default=4)
    ap.add_argument("--no-big", action="store_true", help="场景里不放大图元")
    args = ap.parse_args()

    big = not args.no_big

    for backend in ("d2d", "gdi"):
        samples = run(backend, args.factor, big, args.steps)
        first = samples[0]
        print(f"\n=== {backend} ===")
        print(f"{'step':>5} {'working set':>13} {'private':>13} {'gdi obj':>9} {'user obj':>9}")
        for step, m, g, u in samples:
            print(f"{step:>5} {m['ws']/1024:>10.0f} KB {m['priv']/1024:>10.0f} KB "
                  f"{g:>9} {u:>9}")
        last = samples[-1]
        print(f"  working set  {first[1]['ws']/1024:.0f} KB -> {last[1]['ws']/1024:.0f} KB "
              f"(+{(last[1]['ws']-first[1]['ws'])/1024:.0f} KB)")
        print(f"  private      {first[1]['priv']/1024:.0f} KB -> {last[1]['priv']/1024:.0f} KB "
              f"(+{(last[1]['priv']-first[1]['priv'])/1024:.0f} KB)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
