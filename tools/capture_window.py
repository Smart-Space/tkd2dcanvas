#!/usr/bin/env python3
"""capture_window.py -- 把一个顶层窗口截图保存为 PNG。

用法::

    python tools/capture_window.py <窗口标题子串> <输出 PNG>

脚本会查找标题匹配的可见顶层窗口，然后有两种抓取方式：

* ``PrintWindow``：让窗口自己把内容画到离屏位图上，不受窗口遮挡影响
  （首选方式）；
* 屏幕抓取：把窗口置顶后按屏幕区域抓取（PrintWindow 失败时的兜底）。

依赖 Pillow。
"""

from __future__ import annotations

import ctypes
import sys
import time
from ctypes import wintypes

from PIL import Image, ImageGrab

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32


def _make_dpi_aware() -> None:
    """让本进程感知 DPI，截图时才能拿到物理像素。"""
    try:  # Windows 10 1703+
        ctx = ctypes.c_void_p(-4)  # DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
        if user32.SetProcessDpiAwarenessContext(ctx):
            return
    except (AttributeError, OSError):
        pass
    try:  # Windows Vista+
        user32.SetProcessDPIAware()
    except OSError:
        pass


_make_dpi_aware()


class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [
        ("biSize", wintypes.DWORD),
        ("biWidth", ctypes.c_long),
        ("biHeight", ctypes.c_long),
        ("biPlanes", wintypes.WORD),
        ("biBitCount", wintypes.WORD),
        ("biCompression", wintypes.DWORD),
        ("biSizeImage", wintypes.DWORD),
        ("biXPelsPerMeter", ctypes.c_long),
        ("biYPelsPerMeter", ctypes.c_long),
        ("biClrUsed", wintypes.DWORD),
        ("biClrImportant", wintypes.DWORD),
    ]


class BITMAPINFO(ctypes.Structure):
    _fields_ = [("bmiHeader", BITMAPINFOHEADER), ("bmiColors", wintypes.DWORD * 3)]


def find_window(title: str) -> int | None:
    """返回标题包含 title 的第一个可见顶层窗口的句柄。"""
    found: list[int] = []

    @ctypes.WINFUNCTYPE(ctypes.c_bool, wintypes.HWND, wintypes.LPARAM)
    def callback(hwnd, _lparam):
        if not user32.IsWindowVisible(hwnd):
            return True
        length = user32.GetWindowTextLengthW(hwnd)
        if length == 0:
            return True
        buf = ctypes.create_unicode_buffer(length + 1)
        user32.GetWindowTextW(hwnd, buf, length + 1)
        if title.lower() in buf.value.lower():
            found.append(hwnd)
            return False
        return True

    user32.EnumWindows(callback, 0)
    return found[0] if found else None


def window_rect(hwnd: int) -> tuple[int, int, int, int]:
    rect = wintypes.RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(rect))
    return rect.left, rect.top, rect.right, rect.bottom


def raise_window(hwnd: int) -> None:
    """把窗口带到最前面，避免截图时被其它窗口挡住。"""
    user32.ShowWindow(hwnd, 9)  # SW_RESTORE
    HWND_TOPMOST = -1
    SWP_NOMOVE = 0x0002
    SWP_NOSIZE = 0x0001
    SWP_NOACTIVATE = 0x0010
    user32.SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE)
    user32.BringWindowToTop(hwnd)


def unraise_window(hwnd: int) -> None:
    HWND_NOTOPMOST = -2
    SWP_NOMOVE = 0x0002
    SWP_NOSIZE = 0x0001
    SWP_NOACTIVATE = 0x0010
    user32.SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE)


def print_window(hwnd: int) -> Image.Image | None:
    """用 PrintWindow 把窗口内容画进离屏位图，与遮挡无关。"""
    left, top, right, bottom = window_rect(hwnd)
    width, height = right - left, bottom - top
    if width <= 0 or height <= 0:
        return None

    window_dc = user32.GetWindowDC(hwnd)
    if not window_dc:
        return None
    mem_dc = gdi32.CreateCompatibleDC(window_dc)
    bitmap = gdi32.CreateCompatibleBitmap(window_dc, width, height)
    old = gdi32.SelectObject(mem_dc, bitmap)
    try:
        # PW_RENDERFULLCONTENT = 0x00000002
        ok = user32.PrintWindow(hwnd, mem_dc, 2)
        if not ok:
            ok = user32.PrintWindow(hwnd, mem_dc, 0)
        if not ok:
            return None

        info = BITMAPINFO()
        info.bmiHeader.biSize = ctypes.sizeof(BITMAPINFOHEADER)
        info.bmiHeader.biWidth = width
        info.bmiHeader.biHeight = -height  # top-down
        info.bmiHeader.biPlanes = 1
        info.bmiHeader.biBitCount = 32
        info.bmiHeader.biCompression = 0  # BI_RGB

        buffer = ctypes.create_string_buffer(width * height * 4)
        if not gdi32.GetDIBits(mem_dc, bitmap, 0, height, buffer,
                               ctypes.byref(info), 0):
            return None
        return Image.frombuffer("RGBA", (width, height), buffer, "raw",
                                 "BGRA", 0, 1).convert("RGB")
    finally:
        gdi32.SelectObject(mem_dc, old)
        gdi32.DeleteObject(bitmap)
        gdi32.DeleteDC(mem_dc)
        user32.ReleaseDC(hwnd, window_dc)


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        print(__doc__)
        return 2
    title, out = argv[1], argv[2]

    hwnd = None
    for _ in range(50):
        hwnd = find_window(title)
        if hwnd:
            break
        time.sleep(0.2)
    if not hwnd:
        print(f"找不到标题包含 {title!r} 的窗口")
        return 1

    image = print_window(hwnd)
    if image is not None:
        image.save(out)
        print(f"已保存 {out} ({image.width}x{image.height})，使用 PrintWindow")
        return 0

    left, top, right, bottom = window_rect(hwnd)
    raise_window(hwnd)
    time.sleep(0.7)
    left, top, right, bottom = window_rect(hwnd)
    image = ImageGrab.grab(bbox=(left, top, right, bottom), all_screens=True)
    unraise_window(hwnd)
    image.save(out)
    print(f"已保存 {out} ({image.width}x{image.height})，使用屏幕抓取")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
