"""TinUI ``add_button2``（圆角按钮）在 tkd2dcanvas 上的完整复现。

运行方式（单文件，双击即可）::

    python tests/button2.py

本文件只从 ``tinui/TinUI.py`` 复制了 ``BasicTinUI.add_button2`` 及其必需的
辅助代码（``FuncList`` / ``TinUIString`` / 对齐与多边形辅助函数）。

item ``-state``（``normal`` / ``disabled`` / ``hidden``）与
``-disabledfill`` 均已由 tkd2dcanvas 原生实现，因此
``disable()`` / ``active()`` 与空文本隐藏分支严格沿用 TinUI 原写法：

* ``-state disable``：item 不再参与拾取（悬停 / 按下 / 松开不触发），
  ``-disabledfill`` 指定禁用时的填充色，达到与 TinUI 一致的效果
  （灰字、灰底、不可点击）。
* ``-state hidden``：空文本不绘制、不命中、bbox 为空。

对外 API 与 TinUI 完全一致::

    button, back, outline, funcs, uid = add_button2(canvas, pos, text, ...)
    funcs.change_command(new_func)   # 替换点击回调
    funcs.disable()                   # 禁用按钮
    funcs.active()                    # 恢复按钮
    uid.layout(x1, y1, x2, y2, expand=False)   # 重新布局

``button`` 为文本 item（无文本时为 None），``back`` / ``outline`` 为背景与
描边多边形，``funcs`` 为携带上述三个函数的列表，``uid`` 为该按钮整体的
tag 字符串。
"""

from __future__ import annotations

import inspect
import os
import sys
import tkinter
import tkinter.font as tkfont

# 让仓库内的 tkd2dcanvas 可以被直接导入（双击运行时也有效）。
_REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_PYTHON = os.path.join(_REPO, "python")
if _PYTHON not in sys.path:
    sys.path.insert(0, _PYTHON)

from tkd2dcanvas import Canvas  # noqa: E402


# ---------------------------------------------------------------------------
# tinui/TinUI.py 中的辅助类型（原样复制）
# ---------------------------------------------------------------------------
class FuncList(list):  # 控件的函数列表类
    def __init__(self, num: int = 10):
        list.__init__(self)
        for _ in range(0, num + 1):
            self.append(None)


class TinUIString(str):  # TinUI字符串类
    def __init__(self, string: str = ""):
        str.__init__(self)
        self.text = string


# ---------------------------------------------------------------------------
# tinui/TinUI.py 中的对齐辅助函数（原样复制）
# ---------------------------------------------------------------------------
def _anchor_nw(bbox, pos):
    return pos[0] - bbox[0], pos[1] - bbox[1]


def _anchor_n(bbox, pos):
    return pos[0] - (bbox[0] + bbox[2]) / 2, pos[1] - bbox[1]


def _anchor_ne(bbox, pos):
    return pos[0] - bbox[2], pos[1] - bbox[1]


def _anchor_e(bbox, pos):
    return pos[0] - bbox[2], pos[1] - (bbox[1] + bbox[3]) / 2


def _anchor_se(bbox, pos):
    return pos[0] - bbox[2], pos[1] - bbox[3]


def _anchor_s(bbox, pos):
    return pos[0] - (bbox[0] + bbox[2]) / 2, pos[1] - bbox[3]


def _anchor_sw(bbox, pos):
    return pos[0] - bbox[0], pos[1] - bbox[3]


def _anchor_w(bbox, pos):
    return pos[0] - bbox[0], pos[1] - (bbox[1] + bbox[3]) / 2


def _anchor_center(bbox, pos):
    return pos[0] - (bbox[0] + bbox[2]) / 2, pos[1] - (bbox[1] + bbox[3]) / 2


_ANCHOR_DICT = {
    "nw": _anchor_nw,
    "n": _anchor_n,
    "ne": _anchor_ne,
    "e": _anchor_e,
    "se": _anchor_se,
    "s": _anchor_s,
    "sw": _anchor_sw,
    "w": _anchor_w,
    "center": _anchor_center,
}


def _layout_nw(bbox, bboxuid):
    return bbox[0] - bboxuid[0], bbox[1] - bboxuid[1]


def _layout_n(bbox, bboxuid):
    return (bbox[0] + bbox[2]) / 2 - (bboxuid[0] + bboxuid[2]) / 2, bbox[1] - bboxuid[1]


def _layout_ne(bbox, bboxuid):
    return bbox[2] - bboxuid[2], bbox[1] - bboxuid[1]


def _layout_e(bbox, bboxuid):
    return bbox[2] - bboxuid[2], (bbox[1] + bbox[3]) / 2 - (bboxuid[1] + bboxuid[3]) / 2


def _layout_se(bbox, bboxuid):
    return bbox[2] - bboxuid[2], bbox[3] - bboxuid[3]


def _layout_s(bbox, bboxuid):
    return (bbox[0] + bbox[2]) / 2 - (bboxuid[0] + bboxuid[2]) / 2, bbox[3] - bboxuid[3]


def _layout_sw(bbox, bboxuid):
    return bbox[0] - bboxuid[0], bbox[3] - bboxuid[3]


def _layout_w(bbox, bboxuid):
    return bbox[0] - bboxuid[0], (bbox[1] + bbox[3]) / 2 - (bboxuid[1] + bboxuid[3]) / 2


def _layout_center(bbox, bboxuid):
    return (bbox[0] + bbox[2]) / 2 - (bboxuid[0] + bboxuid[2]) / 2, (
        bbox[1] + bbox[3]
    ) / 2 - (bboxuid[1] + bboxuid[3]) / 2


_LAYOUT_DICT = {
    "nw": _layout_nw,
    "n": _layout_n,
    "ne": _layout_ne,
    "e": _layout_e,
    "se": _layout_se,
    "s": _layout_s,
    "sw": _layout_sw,
    "w": _layout_w,
    "center": _layout_center,
}


# ---------------------------------------------------------------------------
# tinui/TinUI.py 中的画布辅助函数（改为模块级函数，接收 canvas 作为参数）
# ---------------------------------------------------------------------------
def _auto_anchor(canvas, uid, pos, anchor="nw"):
    """统一对齐方式：把 ``uid``（item 或 tag）按 ``anchor`` 对齐到 ``pos``。"""
    bbox = canvas.bbox(uid)
    if bbox is None:
        return 0.0, 0.0
    dx, dy = _ANCHOR_DICT.get(anchor, _ANCHOR_DICT["nw"])(bbox, pos)
    canvas.move(uid, dx, dy)
    return dx, dy


def _auto_layout(canvas, uid, bbox, anchor="center"):
    """把 ``uid`` 移动到目标矩形 ``bbox`` 的对应位置。"""
    bboxuid = canvas.bbox(uid)
    if bboxuid is None:
        return 0.0, 0.0
    dx, dy = _LAYOUT_DICT.get(anchor, _LAYOUT_DICT["center"])(bbox, bboxuid)
    canvas.move(uid, dx, dy)
    return dx, dy


def _ui_polygon(canvas, point2, fill="white", outline="black", width=1, tags=None):
    """以两个对角点绘制矩形多边形（与 TinUI 的 ``__ui_polygon`` 一致）。"""
    points = (
        *point2[0],
        point2[1][0],
        point2[0][1],
        *point2[1],
        point2[0][0],
        point2[1][1],
    )
    return canvas.create_polygon(
        points, fill=fill, outline=outline, width=width, tags=tags
    )


def _scale_value(canvas, value):
    """TinUI 的 ``scale_value``：按画布缩放比例换算尺寸。"""
    scale = getattr(canvas, "TINUISCALE", 1.0)
    return int(value * scale)


def _get_font(canvas, delta=0):
    """TinUI 的 ``__get_font``：默认字体。"""
    family = getattr(canvas, "TINUIFONT", "微软雅黑")
    size = getattr(canvas, "TINUIFONTSIZE", 12)
    return f"{family} {size + delta}"


def _radius_small(canvas):
    """圆角半径（TinUI 的 ``TINUI_RADIUS_SMALL``）。"""
    return getattr(canvas, "TINUI_RADIUS_SMALL", 9)


def _invoke(command, event):
    """执行点击回调，同时兼容 ``command(event)`` 与 ``command()`` 两种写法。"""
    if command is None:
        return
    try:
        signature = inspect.signature(command)
    except (TypeError, ValueError):  # 内置函数等无法取签名
        signature = None
    if signature is not None:
        try:
            signature.bind(event)
        except TypeError:
            command()
            return
    command(event)


# ---------------------------------------------------------------------------
# add_button2 —— TinUI BasicTinUI.add_button2 的复现
# ---------------------------------------------------------------------------
def add_button2(
    canvas,
    pos: tuple,
    text: str,
    icon=None,
    compound="left",
    fg="#1b1b1b",
    bg="#fbfbfb",
    line="#CCCCCC",
    linew=1,
    activefg="#1a1a1a",
    activebg="#f6f6f6",
    activeline="#cccccc",
    onfg="#5d5d5d",
    onbg="#f5f5f5",
    online="#e5e5e5",
    font=None,
    minwidth=0,
    maxwidth=0,
    command=None,
    anchor="nw",
):
    """绘制圆角按钮（与 ``tinui.TinUI.BasicTinUI.add_button2`` 完全一致）。

    返回 ``(button, back, outline, funcs, uid)``：

    * ``button``：文本 item（没有文本时为 None）
    * ``back``：按钮背景多边形
    * ``outline``：按钮描边多边形
    * ``funcs``：带有 ``change_command`` / ``disable`` / ``active`` 的函数列表
    * ``uid``：按钮整体的 tag 字符串，带 ``layout`` 方法
    """
    state = {"mouse_in": False, "disabled": False}

    def in_button(_event=None):
        if state["disabled"]:
            return
        state["mouse_in"] = True
        canvas.itemconfig(outline, outline=activeline, fill=activeline)
        canvas.itemconfig(back, fill=activebg, outline=activebg)
        canvas.itemconfig(buttonuid, fill=activefg)

    def out_button(_event=None):
        if state["disabled"]:
            return
        state["mouse_in"] = False
        canvas.itemconfig(back, fill=bg, outline=bg)
        canvas.itemconfig(outline, outline=line, fill=line)
        canvas.itemconfig(buttonuid, fill=fg)

    def on_click(_event=None):
        if state["disabled"]:
            return
        canvas.itemconfig(back, fill=onbg, outline=onbg)
        canvas.itemconfig(buttonuid, fill=onfg)
        canvas.itemconfig(outline, outline=online, fill=online)

    def out_click(event=None):
        if state["disabled"]:
            return
        if state["mouse_in"]:
            in_button(event)
        else:
            out_button(event)
        _invoke(command, event)

    def change_command(new_func):
        nonlocal command
        command = new_func

    def disable(fg="#9d9d9d", bg="#f5f5f5"):
        """禁用按钮（与 TinUI 一致：item ``-state`` 为 disable）。

        禁用后 item 不再参与拾取（悬停/按下/松开不会触发），
        ``-disabledfill`` 使 back 以禁用填充色绘制，实现与 TinUI 相同的效果。
        """
        state["disabled"] = True
        state["mouse_in"] = False
        canvas.itemconfig(buttonuid, state="disable", fill=fg)
        canvas.itemconfig(back, state="disable", disabledfill=bg)
        canvas.itemconfig(outline, state="disable")

    def active():
        """恢复被禁用的按钮。"""
        state["disabled"] = False
        canvas.itemconfig(buttonuid, state="normal")
        canvas.itemconfig(back, state="normal")
        canvas.itemconfig(outline, state="normal")
        out_button(None)

    def __layout(x1, y1, x2, y2, expand=False):
        if not expand:
            _auto_layout(canvas, uid, (x1, y1, x2, y2), anchor)
        else:
            _auto_anchor(canvas, buttonuid, ((x1 + x2) / 2, (y1 + y2) / 2), "center")
            scale_5 = _scale_value(canvas, 5)
            scale_4 = _scale_value(canvas, 4)
            canvas.coords(
                back,
                x1 + scale_5, y1 + scale_5,
                x2 - scale_5, y1 + scale_5,
                x2 - scale_5, y2 - scale_5,
                x1 + scale_5, y2 - scale_5,
            )
            canvas.coords(
                outline,
                x1 + scale_4, y1 + scale_4,
                x2 - scale_4, y1 + scale_4,
                x2 - scale_4, y2 - scale_4,
                x1 + scale_4, y2 - scale_4,
            )

    font = font or _get_font(canvas)
    font = tkfont.Font(font=font)
    # 保持字体对象的引用，否则 tkinter 会把它删除，文本会退回默认字体。
    _keep_alive(canvas, "fonts", font)

    button = canvas.create_text(pos, text=text, fill=fg, font=font)
    uid = TinUIString(f"button2-{button}")
    buttonuid = uid + "button"
    canvas.itemconfig(button, tags=(uid, buttonuid))

    icon_img = False
    icontext = None
    if icon:  # Fluent Icons 编码图标，或图片文件路径
        if os.path.isfile(icon):  # 这里要是文件不存在报错，那是开发者的问题
            icon_img = True
            image = tkinter.PhotoImage(file=icon)
            _keep_alive(canvas, "images", image)
            icontext = canvas.create_image(
                pos, image=image, tags=(uid, buttonuid, f"{uid}icon")
            )
        else:
            font_size = font.cget(option="size")
            icontext = canvas.create_text(
                pos,
                text=icon,
                fill=fg,
                font=f"{{Segoe Fluent Icons}} {font_size}",
                tags=(uid, buttonuid, f"{uid}icon"),
            )
        iconbbox = canvas.bbox(icontext)
        if iconbbox is None:
            iconbbox = (0, 0, 0, 0)
        if compound == "left":
            textpos = (iconbbox[2] + 3, (iconbbox[3] + iconbbox[1]) / 2)
            _auto_anchor(canvas, button, textpos, "w")
        elif compound == "right":
            textpos = (iconbbox[0] - 3, (iconbbox[3] + iconbbox[1]) / 2)
            _auto_anchor(canvas, button, textpos, "e")
        elif compound == "top":
            textpos = ((iconbbox[0] + iconbbox[2]) / 2, iconbbox[3] + 3)
            _auto_anchor(canvas, button, textpos, "n")
        elif compound == "bottom":
            textpos = ((iconbbox[0] + iconbbox[2]) / 2, iconbbox[1] - 3)
            _auto_anchor(canvas, button, textpos, "s")
        if text == "":  # 有图标的时候，如果无文本，则隐藏文本元素
            canvas.delete(button)
            button = None

    groupbox = canvas.bbox(buttonuid)
    if groupbox is None:
        groupbox = (0, 0, 0, 0)
    x1, y1, x2, y2 = groupbox
    linew -= 1
    # 判断宽度的极限，分为最大化和最小化
    nowwidth = x2 - x1
    if 0 < maxwidth <= nowwidth and button is not None:
        canvas.itemconfig(button, width=maxwidth)
        bbox = canvas.bbox(button)
        if bbox is not None:
            x1, y1, x2, y2 = bbox
            nowwidth = x2 - x1
    if nowwidth < minwidth:
        dx = minwidth - nowwidth
        x2 += dx / 2
        x1 -= dx / 2

    radius = _radius_small(canvas)
    outline = _ui_polygon(
        canvas,
        ((x1 - linew, y1 - linew), (x2 + linew, y2 + linew)),
        width=radius,
        tags=uid,
        fill=line,
        outline=line,
    )
    back = _ui_polygon(
        canvas,
        ((x1 + 1, y1 + 1), (x2 - 1, y2 - 1)),
        width=radius,
        tags=uid,
        fill=bg,
        outline=bg,
    )

    canvas.tag_bind(uid, "<Button-1>", on_click)
    canvas.tag_bind(uid, "<ButtonRelease-1>", out_click)
    canvas.tag_bind(uid, "<Enter>", in_button)
    canvas.tag_bind(uid, "<Leave>", out_button)
    canvas.tkraise(buttonuid)
    if icon_img:
        canvas.dtag(icontext, buttonuid)
    if text == "" and button is not None:
        # 有文本、无图标的按钮：文本是空串时将文本 item 隐藏
        # （与 TinUI 的 ``-state hidden`` 一致：不绘制、不命中、bbox 为空）。
        canvas.itemconfig(button, state="hidden")

    _auto_anchor(canvas, uid, pos, anchor)

    funcs = FuncList(3)
    funcs.change_command = change_command
    funcs.disable = disable
    funcs.active = active
    funcs.font = font
    uid.layout = __layout
    return button, back, outline, funcs, uid


def _keep_alive(canvas, kind, *values):
    """在 canvas 上保存引用，避免字体 / 图片被垃圾回收。"""
    store = getattr(canvas, "_tinui_refs", None)
    if store is None:
        store = {}
        canvas._tinui_refs = store
    store.setdefault(kind, []).extend(values)


class TinUIButton2Mixin:
    """把 TinUI 的 ``add_button2`` 变成画布的方法（用法与 TinUI 一致）。"""

    TINUIFONT = "微软雅黑"
    TINUIFONTSIZE = 12
    TINUISCALE = 1.0
    TINUI_RADIUS_SMALL = 9
    TINUI_RADIUS_LARGE = 17

    def scale_value(self, value, needodd=False):
        res = int(value * self.TINUISCALE)
        if needodd and res % 2 == 0:
            res += 1
        return res

    def set_scale(self, scale=1.0):
        self.TINUISCALE = scale
        self.TINUI_RADIUS_LARGE = int(self.TINUISCALE * 17)
        if self.TINUI_RADIUS_LARGE % 2 == 0:
            self.TINUI_RADIUS_LARGE += 1
        self.TINUI_RADIUS_SMALL = int(self.TINUISCALE * 9)
        if self.TINUI_RADIUS_SMALL % 2 == 0:
            self.TINUI_RADIUS_SMALL += 1

    def add_button2(
        self,
        pos: tuple,
        text: str,
        icon=None,
        compound="left",
        fg="#1b1b1b",
        bg="#fbfbfb",
        line="#CCCCCC",
        linew=1,
        activefg="#1a1a1a",
        activebg="#f6f6f6",
        activeline="#cccccc",
        onfg="#5d5d5d",
        onbg="#f5f5f5",
        online="#e5e5e5",
        font=None,
        minwidth=0,
        maxwidth=0,
        command=None,
        anchor="nw",
    ):
        """绘制圆角按钮（等价于 ``add_button2(canvas, ...)``）。"""
        return add_button2(
            self,
            pos,
            text,
            icon=icon,
            compound=compound,
            fg=fg,
            bg=bg,
            line=line,
            linew=linew,
            activefg=activefg,
            activebg=activebg,
            activeline=activeline,
            onfg=onfg,
            onbg=onbg,
            online=online,
            font=font,
            minwidth=minwidth,
            maxwidth=maxwidth,
            command=command,
            anchor=anchor,
        )


class Button2Canvas(TinUIButton2Mixin, Canvas):
    """tkd2dcanvas + TinUI 的按钮 API。"""


# ---------------------------------------------------------------------------
# 演示
# ---------------------------------------------------------------------------
def main() -> None:
    root = tkinter.Tk()
    root.title("tkd2dcanvas · TinUI add_button2 复现")

    canvas = Button2Canvas(
        root,
        width=720,
        height=420,
        background="#f3f3f3",
        highlightthickness=0,
        bd=0,
    )
    canvas.pack(padx=10, pady=10)

    canvas.create_text(
        360,
        34,
        text="TinUI add_button2 · 圆角按钮",
        fill="#333333",
        font=("微软雅黑", 15),
        anchor="center",
    )
    canvas.create_text(
        360,
        60,
        text="悬停 / 按下 / 释放时按钮会变色，图标使用 Segoe Fluent Icons 编码",
        fill="#787878",
        font=("微软雅黑", 10),
        anchor="center",
    )

    clicks = {"n": 0}
    counter = canvas.create_text(
        360, 372, text="点击次数：0", fill="#1b1b1b",
        font=("微软雅黑", 12), anchor="center",
    )

    def on_click(_event=None):
        clicks["n"] += 1
        canvas.itemconfigure(counter, text="点击次数：{}".format(clicks["n"]))

    # 1. 普通按钮
    add_button2(canvas, (60, 100), "圆角按钮", command=on_click)

    # 2. 带图标的按钮（compound="left"）
    add_button2(canvas, (230, 100), "带图标", icon="\uE73E",
                compound="left", command=on_click)

    # 3. 禁用的按钮
    _, _, _, funcs_disabled, _ = add_button2(canvas, (400, 100), "禁用按钮")
    funcs_disabled.disable()

    # 4. 图标在上的按钮
    add_button2(canvas, (60, 160), "图标在上", icon="\uE713",
                compound="top", command=on_click)

    # 5. 最小宽度
    add_button2(canvas, (240, 160), "最小宽度 170", minwidth=170, command=on_click)

    # 6. 自定义配色
    add_button2(canvas, (460, 160), "自定义配色", bg="#eef4ff", line="#3b7ddd",
                activebg="#e2ecff", activeline="#2f6fd0", command=on_click)

    # 7. uid.layout()：点击后把按钮铺满指定区域
    holder = {}

    def do_layout(_event=None):
        uid = holder.get("uid")
        if uid is not None:
            uid.layout(60, 240, 460, 296, expand=True)

    _, _, _, _, uid_layout = add_button2(canvas, (60, 240), "点击我：自动布局",
                                          command=do_layout)
    holder["uid"] = uid_layout

    root.mainloop()


if __name__ == "__main__":
    main()
