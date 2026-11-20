"""bench_tk_aa_memory.py — 原生 Tk 画布（-antialias 1）的滚动内存占用。

Tk 侧的抗锯齿引擎在 third_party/tcltk-src/tk/win/tkWinAA.c，编译产物是
third_party/tcltk-src/tk/win/Release_AMD64_VC1951/wish86t.exe。

脚本让画布在同一片区域来回滚动，每一轮打印一行；外面按行采样进程内存。
"""

from __future__ import annotations

import argparse
import ctypes
import ctypes.wintypes as wt
import os
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WISH = REPO / "third_party" / "tcltk-src" / "tk" / "win" / \
    "Release_AMD64_VC1951" / "wish86t.exe"

K32 = ctypes.WinDLL("kernel32", use_last_error=True)
PSAPI = ctypes.WinDLL("psapi", use_last_error=True)
K32.GetCurrentProcess.restype = wt.HANDLE
PSAPI.GetProcessMemoryInfo.argtypes = [wt.HANDLE, ctypes.POINTER(C := type(
    "C", (ctypes.Structure,), {"_fields_": [
        ("cb", wt.DWORD), ("PageFaultCount", wt.DWORD),
        ("PeakWorkingSetSize", ctypes.c_size_t), ("WorkingSetSize", ctypes.c_size_t),
        ("QuotaPeakPagedPoolUsage", ctypes.c_size_t), ("QuotaPagedPoolUsage", ctypes.c_size_t),
        ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t), ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
        ("PagefileUsage", ctypes.c_size_t), ("PeakPagefileUsage", ctypes.c_size_t),
        ("PrivateUsage", ctypes.c_size_t),
    ]})), wt.DWORD]


def mem(pid: int):
    h = K32.OpenProcess(0x1000 | 0x0400, False, pid)  # QUERY_LIMITED_INFO | QUERY_INFO
    if not h:
        return 0, 0
    c = C()
    c.cb = ctypes.sizeof(c)
    PSAPI.GetProcessMemoryInfo(h, ctypes.byref(c), c.cb)
    K32.CloseHandle(h)
    return c.WorkingSetSize, c.PrivateUsage


TCL = r"""
set aa {1}
if {[info exists ::env(NO_AA)]} { set aa 0 }
canvas .c -width 600 -height 400 -background white \
    -scrollregion {0 0 600 6000} -antialias $aa -aafactor 4
pack .c
for {set i 0} {$i < 150} {incr i} {
    set y [expr {10 + $i * 20}]
    .c create oval 10 $y 34 [expr {$y + 24}] -fill #e09090 -outline #803030 -width 2
}
update
after 500
puts "START"
flush stdout
set cycles 40
for {set k 1} {$k <= $cycles} {incr k} {
    .c yview moveto 0.3
    update
    .c yview moveto 0.0
    update
    puts "CYCLE $k"
    flush stdout
    after 120
}
puts "DONE"
flush stdout
exit 0
"""


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cycles", type=int, default=40)
    ap.add_argument("--no-aa", action="store_true", help="关掉 -antialias（原生 GDI 对照）")
    args = ap.parse_args()

    if not WISH.is_file():
        print(f"找不到 {WISH}；先构建 third_party/tcltk-src/tk")
        return 1

    script = REPO / "tests" / "gdi_aa" / "build" / "_tk_aa_scroll.tcl"
    script.parent.mkdir(parents=True, exist_ok=True)
    script.write_text(TCL, encoding="utf-8")

    env = dict(os.environ)
    env["TCL_LIBRARY"] = str(REPO / "third_party" / "tcltk-src" / "tcl" / "library")
    env["TK_LIBRARY"] = str(REPO / "third_party" / "tcltk-src" / "tk" / "library")
    # wish86t.exe 要加载同版本的 tcl86t.dll：它不在 wish 旁边，得靠 PATH。
    tcl_bin = REPO / "third_party" / "tcltk-src" / "tcl" / "win" / \
        "Release_AMD64_VC1951"
    if tcl_bin.is_dir():
        env["PATH"] = str(tcl_bin) + os.pathsep + env.get("PATH", "")
    if args.no_aa:
        env["NO_AA"] = "1"
    proc = subprocess.Popen([str(WISH), str(script)], stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, env=env)
    print(f"wish pid={proc.pid} aa={'off' if args.no_aa else 'on'}")
    for line in proc.stdout:
        line = line.strip()
        if line.startswith("CYCLE") or line in ("START", "DONE"):
            ws, pr = mem(proc.pid)
            print(f"{line:10s} ws={ws/1024:8.0f} KB priv={pr/1024:8.0f} KB", flush=True)
    proc.wait()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
