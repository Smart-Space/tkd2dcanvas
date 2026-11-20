"""diag_memory_map.py -- break the process private commit down by kind.

PrivateUsage is only a single total; it does not say which part grows.
This walks the address space with VirtualQueryEx and splits the committed
private memory into:

  heap  -- private read/write pages (PAGE_READWRITE): the C++/Python heap
  sec   -- private copy-on-write pages (PAGE_WRITECOPY): kernel backed
           sections (DIB sections, section objects)
  img   -- module images
  other -- everything else

Usage:
    python tests/gdi_aa/diag_memory_map.py --backend gdi --cycles 20
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


class MODULEENTRY32(ctypes.Structure):
    _fields_ = [
        ("dwSize", wt.DWORD), ("th32ModuleID", wt.DWORD),
        ("th32ProcessID", wt.DWORD), ("GlblcntUsage", wt.DWORD),
        ("ProccntUsage", wt.DWORD), ("modBaseAddr", ctypes.POINTER(ctypes.c_byte)),
        ("modBaseSize", wt.DWORD), ("hModule", wt.HANDLE),
        ("szModule", wt.WCHAR * 256), ("szExePath", wt.WCHAR * 260),
    ]


K32.CreateToolhelp32Snapshot.argtypes = [wt.DWORD, wt.DWORD]
K32.CreateToolhelp32Snapshot.restype = wt.HANDLE
K32.Module32FirstW.argtypes = [wt.HANDLE, ctypes.POINTER(MODULEENTRY32)]
K32.Module32FirstW.restype = wt.BOOL
K32.Module32NextW.argtypes = [wt.HANDLE, ctypes.POINTER(MODULEENTRY32)]
K32.Module32NextW.restype = wt.BOOL
TH32CS_SNAPMODULE = 0x00000008


def modules() -> list:
    """Loaded modules: list of (name, base, size)."""
    snap = K32.CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0)
    if snap in (0, -1):
        return []
    out = []
    entry = MODULEENTRY32()
    entry.dwSize = ctypes.sizeof(MODULEENTRY32)
    try:
        ok = K32.Module32FirstW(snap, ctypes.byref(entry))
        while ok:
            base = ctypes.cast(entry.modBaseAddr, ctypes.c_void_p).value or 0
            out.append((entry.szModule, base, entry.modBaseSize))
            ok = K32.Module32NextW(snap, ctypes.byref(entry))
    finally:
        K32.CloseHandle(snap)
    return out


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

MEM_COMMIT = 0x1000
MEM_IMAGE = 0x1000000
MEM_MAPPED = 0x40000
MEM_PRIVATE = 0x20000
PAGE_READWRITE = 0x04
PAGE_WRITECOPY = 0x08


def priv_usage() -> int:
    c = C()
    c.cb = ctypes.sizeof(c)
    PSAPI.GetProcessMemoryInfo(K32.GetCurrentProcess(), ctypes.byref(c), c.cb)
    return c.PrivateUsage


def address_space(mods=None) -> dict:
    """Split the committed private memory into buckets (bytes).

    When `mods` is given, the private read/write pages are additionally
    attributed to the module that owns them (the heap of a module lives
    inside that module's address range).
    """
    buckets = {"heap": 0, "sec": 0, "img": 0, "mapped": 0, "other": 0}
    if mods is None:
        mods = []
    per_mod = {}
    addr = 0
    limit = 0x7FFFFFFF0000
    mbi = MBI()
    while addr < limit:
        n = K32.VirtualQueryEx(K32.GetCurrentProcess(), ctypes.c_void_p(addr),
                               ctypes.byref(mbi), ctypes.sizeof(mbi))
        if not n:
            break
        base = mbi.BaseAddress or 0
        size = mbi.RegionSize
        if mbi.State == MEM_COMMIT:
            prot = mbi.Protect & 0xFF
            if mbi.Type == MEM_IMAGE:
                buckets["img"] += size
            elif mbi.Type == MEM_MAPPED:
                buckets["mapped"] += size
            elif mbi.Type == MEM_PRIVATE:
                if prot == PAGE_READWRITE:
                    buckets["heap"] += size
                    alloc = mbi.AllocationBase or 0
                    for name, mbase, msize in mods:
                        if mbase <= alloc < mbase + msize:
                            per_mod[name] = per_mod.get(name, 0) + size
                            break
                elif prot == PAGE_WRITECOPY:
                    buckets["sec"] += size
                else:
                    buckets["other"] += size
            else:
                buckets["other"] += size
        nxt = base + size
        if nxt <= addr:
            break
        addr = nxt
    buckets["_per_mod"] = per_mod
    return buckets


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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--backend", default="gdi")
    ap.add_argument("--cycles", type=int, default=20)
    args = ap.parse_args()

    sys.path.insert(0, str(HERE))
    from run_tinui import canvas_class  # noqa: E402

    import tkinter
    tkinter.Canvas = canvas_class(args.backend)

    def measure():
        import tkinter
        root = tkinter._default_root
        canvases = find_canvases(root, [])
        scrollable = []
        for cv in canvases:
            try:
                region = str(cv.cget("scrollregion"))
            except tkinter.TclError:
                continue
            parts = region.split()
            if len(parts) == 4:
                try:
                    h = float(parts[3])
                except ValueError:
                    continue
                if h > cv.winfo_height() + 4:
                    scrollable.append(cv)

        mods = modules()

        def snap(tag):
            m = address_space(mods)
            line = ("[{0}] {1:>10}  priv={2:8.0f} KB  heap={3:8.0f} KB  "
                    "sec={4:8.0f} KB  img={5:8.0f} KB  other={6:8.0f} KB").format(
                        args.backend, tag, priv_usage() / 1024, m["heap"] / 1024,
                        m["sec"] / 1024, m["img"] / 1024,
                        m["mapped"] / 1024 + m["other"] / 1024)
            print(line, flush=True)
            pm = m["_per_mod"]
            for name in sorted(pm, key=lambda k: -pm[k]):
                if pm[name] >= 64 * 1024:
                    print("        {0:<28} {1:8.0f} KB".format(name, pm[name] / 1024),
                          flush=True)
            return m

        snap("start")
        time.sleep(1.0)
        root.update()
        snap("idle")
        base = None
        for cycle in range(1, args.cycles + 1):
            for frac in (0.3, 0.0):
                for cv in scrollable:
                    try:
                        cv.yview_moveto(frac)
                    except tkinter.TclError:
                        pass
                root.update()
            root.update()
            m = snap("cycle {0:3d}".format(cycle))
            if base is None:
                base = m
            else:
                d = dict((k, (m[k] - base[k]) // 1024) for k in
                         ("heap", "sec", "img", "mapped", "other"))
                line = ("[{0}] {1:>10}  dheap={2:+7d} KB  dsec={3:+7d} KB  "
                        "dimg={4:+7d} KB  dother={5:+7d} KB").format(
                            args.backend, "delta",
                            d["heap"], d["sec"], d["img"],
                            d["mapped"] + d["other"])
                print(line, flush=True)
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


if __name__ == "__main__":
    main()
