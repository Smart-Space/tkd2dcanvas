"""diag_d2d_growth.py -- where does the Direct2D back end's growth come from?

The TinUI demo grows by ~30 MB while it is scrolled back and forth with the
Direct2D back end.  `diag_memory_map.py` shows that the growth is all in the
process heap (private read/write pages), not in DIB sections and not in
module images -- so it is a heap allocation somewhere.

Two suspects:

  * **the DirectWrite glyph cache**: every glyph that is drawn for the
    first time is rasterized into the glyph atlas of the D2D device.
    Scrolling a list brings new text into view, so the atlas keeps growing.
  * **the D2D back end's own bitmap cache**: every image item is cached as
    an `ID2D1Bitmap`, keyed by the item id (`ImageItem::GetId()`).  The
    ids are unique per canvas, so every item that scrolls into view adds
    another bitmap to the cache of that widget.

This script tells the two apart:

  * `--scene tinui` runs the real demo and hides whole classes of items
    (`--hide text`, `--hide image`, `--hide both`).  If the growth
    disappears when the text items are hidden, it is the glyph cache; if it
    disappears when the images are hidden, it is the bitmap cache.
  * `--scene min` builds a controlled scene that contains nothing but one
    kind of item (text / image / shape).

Usage:
    python tests/gdi_aa/diag_d2d_growth.py --scene tinui --hide text --cycles 40
    python tests/gdi_aa/diag_d2d_growth.py --scene min --kind text --items 300
"""

from __future__ import annotations

import argparse
import ctypes
import ctypes.wintypes as wt
import os
import runpy
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
HERE = REPO / "tests" / "tinui"
sys.path.insert(0, str(REPO / "python"))

try:
    ctypes.windll.shcore.SetProcessDpiAwareness(2)
except Exception:  # noqa: BLE001
    pass

K32 = ctypes.WinDLL("kernel32", use_last_error=True)
PSAPI = ctypes.WinDLL("psapi", use_last_error=True)
K32.GetCurrentProcess.restype = wt.HANDLE


class C(ctypes.Structure):
    _fields_ = [
        ("cb", wt.DWORD), ("PageFaultCount", wt.DWORD),
        ("PeakWorkingSetSize", ctypes.c_size_t), ("WorkingSetSize", ctypes.c_size_t),
        ("QuotaPeakPagedPoolUsage", ctypes.c_size_t), ("QuotaPagedPoolUsage", ctypes.c_size_t),
        ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t), ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
        ("PagefileUsage", ctypes.c_size_t), ("PeakPagefileUsage", ctypes.c_size_t),
        ("PrivateUsage", ctypes.c_size_t),
    ]


PSAPI.GetProcessMemoryInfo.argtypes = [wt.HANDLE, ctypes.POINTER(C), wt.DWORD]


class MBI(ctypes.Structure):
    _fields_ = [
        ("BaseAddress", ctypes.c_void_p),
        ("AllocationBase", ctypes.c_void_p),
        ("AllocationProtect", wt.DWORD),
        ("PartitionId", wt.WORD),
        ("RegionSize", ctypes.c_size_t),
        ("State", wt.DWORD),
        ("Protect", wt.DWORD),
        ("Type", wt.DWORD),
    ]


K32.VirtualQueryEx.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.POINTER(MBI),
                                ctypes.c_size_t]
K32.VirtualQueryEx.restype = ctypes.c_size_t


def priv() -> int:
    c = C()
    c.cb = ctypes.sizeof(c)
    PSAPI.GetProcessMemoryInfo(K32.GetCurrentProcess(), ctypes.byref(c), c.cb)
    return c.PrivateUsage


def heap_bytes() -> int:
    """Committed private read/write pages (the heap of every module)."""
    total = 0
    addr = 0
    mbi = MBI()
    while addr < 0x7FFFFFFF0000:
        if not K32.VirtualQueryEx(K32.GetCurrentProcess(), ctypes.c_void_p(addr),
                                  ctypes.byref(mbi), ctypes.sizeof(mbi)):
            break
        base = mbi.BaseAddress or 0
        size = mbi.RegionSize
        if (mbi.State == 0x1000 and mbi.Type == 0x20000 and
                (mbi.Protect & 0xFF) == 0x04):
            total += size
        nxt = base + size
        if nxt <= addr:
            break
        addr = nxt
    return total


def find_canvases(widget, out):
    import tkinter
    for child in widget.winfo_children():
        if isinstance(child, tkinter.Canvas):
            out.append(child)
        try:
            find_canvases(child, out)
        except tkinter.TclError:
            pass
    return out


def hide_items(canvases, kinds) -> int:
    """Hides every item of the given kinds ('text', 'image', 'bitmap')."""
    hidden = 0
    for cv in canvases:
        try:
            ids = cv.find_all()
        except tkinter.TclError:
            continue
        for item in ids:
            try:
                kind = str(cv.type(item))
            except tkinter.TclError:
                continue
            if kind not in kinds:
                continue
            try:
                cv.itemconfigure(item, state="hidden")
                hidden += 1
            except tkinter.TclError:
                pass
    return hidden


def scrollable_of(canvases):
    out = []
    for cv in canvases:
        try:
            region = str(cv.cget("scrollregion"))
        except tkinter.TclError:
            continue
        parts = region.split()
        if len(parts) != 4:
            continue
        try:
            h = float(parts[3])
        except ValueError:
            continue
        if h > cv.winfo_height() + 4:
            out.append(cv)
    return out


def report(tag, base):
    p = priv()
    h = heap_bytes()
    print("[{0}] {1:<12} priv={2:8.0f} KB d={3:+8.0f} KB  heap={4:8.0f} KB "
          "d={5:+8.0f} KB".format(args.backend, tag, p / 1024, (p - base) / 1024,
                                  h / 1024, (h - base_heap[0]) / 1024), flush=True)


base_heap = [0]


# ---------------------------------------------------------------------------
# Scene 1: the real demo, with whole classes of items hidden
# ---------------------------------------------------------------------------

def run_tinui(args):
    sys.path.insert(0, str(HERE))
    from run_tinui import canvas_class  # noqa: E402

    import tkinter
    tkinter.Canvas = canvas_class(args.backend)

    def measure():
        root = tkinter._default_root
        canvases = find_canvases(root, [])
        kinds = set()
        if args.hide in ("text", "both"):
            kinds.add("text")
        if args.hide in ("image", "both"):
            kinds.update(("image", "bitmap"))
        hidden = hide_items(canvases, kinds) if kinds else 0
        scrollable = scrollable_of(canvases)
        print("[{0}] 画布 {1} 个，可滚动 {2} 个，隐藏了 {3} 个图元".format(
            args.backend, len(canvases), len(scrollable), hidden), flush=True)

        base = priv()
        base_heap[0] = heap_bytes()
        report("start", base)
        for cycle in range(1, args.cycles + 1):
            for frac in (0.3, 0.0):
                for cv in scrollable:
                    try:
                        cv.yview_moveto(frac)
                    except tkinter.TclError:
                        pass
                root.update()
            root.update()
            report("cycle {0}".format(cycle), base)
        root.destroy()

    import tkinter
    orig_init = tkinter.Tk.__init__

    def __init__(self, *a, **kw):
        orig_init(self, *a, **kw)
        self.after(2500, measure)

    tkinter.Tk.__init__ = __init__

    cwd = os.getcwd()
    os.chdir(HERE)
    try:
        runpy.run_path(str(HERE / "TinUI.py"), run_name="__main__")
    finally:
        os.chdir(cwd)


# ---------------------------------------------------------------------------
# Scene 2: a controlled scene with one kind of item
# ---------------------------------------------------------------------------

def run_min(args):
    import tkinter

    if args.backend == "native":
        canvas_cls = tkinter.Canvas
    else:
        from tkd2dcanvas import Canvas as canvas_cls

    root = tkinter.Tk()
    root.geometry("800x600+40+40")
    options = {} if args.backend == "native" else {"renderer": args.backend}
    # The scroll region has to hold every item, otherwise the items below
    # the fold are never drawn (and their glyphs never reach the atlas).
    region_height = 40 + args.items * 18 + 200
    cv = canvas_cls(root, width=800, height=600, background="white",
                    scrollregion=(0, 0, 800, region_height), **options)
    cv.pack()

    if args.kind == "text":
        # Every item has its own text, so every item brings new glyphs.
        for i in range(args.items):
            y = 40 + i * 18
            cv.create_text(20, y, anchor="nw",
                           text="条目 {0} 内存增长测试 {0}".format(i),
                           fill="#202020")
    elif args.kind == "text-cjk":
        # Every item brings a *new* character (CJK codepoints 0x4E00+):
        # the glyph atlas of the D2D device has to hold all of them.
        for i in range(args.items):
            y = 40 + i * 18
            cv.create_text(20, y, anchor="nw",
                           text="测{0}".format(chr(0x4E00 + i)), fill="#202020")
    elif args.kind == "text-same":
        # The same text everywhere: the glyph set never grows.
        for i in range(args.items):
            y = 40 + i * 18
            cv.create_text(20, y, anchor="nw",
                           text="条目 内存增长测试", fill="#202020")
    elif args.kind == "image":
        # Every item gets its own photo (a different colour), so every item
        # brings a new bitmap into the cache of the widget.
        for i in range(args.items):
            y = 40 + i * 18
            img = tkinter.PhotoImage(width=args.image_size, height=args.image_size)
            img.put("#{0:02x}{1:02x}{2:02x}".format((i * 7) % 256,
                                                     (i * 13) % 256,
                                                     (i * 29) % 256),
                    to=(0, 0, args.image_size, args.image_size))
            cv.create_image(20, y, anchor="nw", image=img)
    else:
        for i in range(args.items):
            x = 20 + (i % 10) * 70
            y = 40 + (i // 10) * 60
            cv.create_oval(x, y, x + 50, y + 50, fill="#4080c0", outline="")

    root.update()
    root.update()

    base = priv()
    base_heap[0] = heap_bytes()
    report("start", base)
    for cycle in range(1, args.cycles + 1):
        for frac in (0.3, 0.0):
            cv.yview_moveto(frac)
            root.update()
        root.update()
        report("cycle {0}".format(cycle), base)
    root.destroy()


def main():
    global args
    ap = argparse.ArgumentParser()
    ap.add_argument("--backend", default="d2d")
    ap.add_argument("--scene", default="tinui", choices=("tinui", "min"))
    ap.add_argument("--hide", default="none",
                    choices=("none", "text", "image", "both"))
    ap.add_argument("--kind", default="shape",
                    choices=("text", "text-cjk", "text-same", "image", "shape"))
    ap.add_argument("--items", type=int, default=200)
    ap.add_argument("--image-size", type=int, default=64)
    ap.add_argument("--cycles", type=int, default=40)
    args = ap.parse_args()

    if args.scene == "tinui":
        run_tinui(args)
    else:
        run_min(args)


if __name__ == "__main__":
    main()
