# 架构说明

> 原始规划见 [规划.md](规划.md)。
> **当前实现**：渲染后端为 **Direct2D / DirectWrite**（`native/render/`），
> 其余分层与规划一致。变更决策记录见 [performance.md](performance.md)。

## 总体结构

```text
Python/tkinter
    │  root.tk.call("tkd2dcanvas", ".c", ...) / root.tk.call(".c", "create", ...)
    v
Tcl interpreter
    │  package require tkd2dcanvas
    v
C/C++ Tk widget (native/tk_glue)
    │  CanvasModel 更新；Win32Surface invalidate
    v
Win32 surface / HWND / WM_PAINT (native/platform_win)
    v
Direct2D renderer + DirectWrite (native/render)
```

## 分层与依赖方向

| 层 | 目录 | 依赖 |
| --- | --- | --- |
| 核心模型 | `native/include` + `native/core` | 无 |
| Item 实现 | `native/items` | core |
| 渲染抽象/实现 | `native/render` | core、d2d1/dwrite |
| Win32 平台 | `native/platform_win` | Win32、render |
| Tk 胶水 | `native/tk_glue` | Tcl/Tk、core、platform_win |
| Tcl package | `tcl/`、`python/.../tcl/` | Tcl |
| Python 包装 | `python/tkd2dcanvas/` | Python/tkinter |

规则：**依赖只能向下**。`core` 不知道 D2D 和 Tcl 是什么；`render` 不知道
Tcl/Tk；`platform_win` 不知道 Tcl；`tk_glue` 是唯一同时认识两边的层。

## 各模块要点

- `CanvasModel`：item 存储（z-order 向量 + id→item 映射）、tag 索引、
  视图/滚动状态（`CanvasView`）、bbox/hit-test 派发。项目的灵魂。
- `TagsIndex`：tag ↔ item 双向索引。
- `ItemFactory`：`"rectangle"` 等字符串 → `CanvasItemPtr`。
- `Renderer`（`native/include/tkd2dcanvas/Renderer.h`）：抽象接口
  （`clear / drawLine / fillPath / drawText / drawImage / pushClip / popClip`）。
  `D2DRenderer`（`native/render/D2DRenderer.cpp`）实现它；将来若需第三种后端，
  只需再实现该接口。
- `D2DRenderer`：
  - 绑定内存 HDC：`ID2D1DCRenderTarget` + 32bpp top-down DIB section
    （`Win32DoubleBuffer` 按此实现）→ 渲染完成后 `BitBlt` 到目标 DC，无闪烁。
  - 平滑曲线：`-smooth` 走 `SmoothCurve`（`native/core`，直译 Tk 的`TkMakeBezierCurve`/`TkMakeRawCurve`）预先展开成折线再交给 D2D。
  - 虚线：`PenCache` 缓存 `ID2D1StrokeStyle`；D2D 的 dash 数组与dashOffset 均以 stroke width 为单位，`PenCache` 把 Tk 的绝对canvas 单位（-dash / -dashoffset）除以线宽后传入。
  - 文字：DirectWrite（`IDWriteTextLayout`，按行绘制，支持
    bold/italic/underline/strikethrough 与 Tk anchor 九宫格定位），
    字体走 `FontCache`（`IDWriteTextFormat` 缓存）。
  - 裁剪：`SetAxisAlignedClip` / `ResetAxisAlignedClip`，支持嵌套 push/pop
    （`pushClip` 取交集，`popClip` 恢复）。
  - 抗锯齿：`SetAntialiasMode(PER_PRIMITIVE)`，文字 ClearType。
  - 断设备（`D2DERR_RECREATE_TARGET`）：自动丢弃 render target，下一帧重建。
- `Win32Surface`：32bpp 双缓冲 + D2D 绘制 + `BitBlt`；由 Tk 的Expose/Configure 事件驱动 `Paint()`。`WM_PAINT` 使用精确的 update region 矩形列表（不是包围盒）；滚动时先用 `ScrollWindowEx` + 后备缓冲`ScrollContent` 平移像素，再只重画新暴露条带。
- `D2DRenderer` 的 DC 绑定在 HDC/尺寸不变时跨帧复用；后备缓冲重建时由`Win32Surface` 调 `ResetBinding()` 强制重新 `BindDC`。
- `tk_glue`：`Tkd2dcanvas_Init`（package 入口，初始化 D2D/DWrite
  factory）、`tkd2dcanvas` 全局命令、`CanvasWidget`、各子命令分发。
- `ItemRegistrations.cpp`：把 `rectangle`/`oval`/`line` 注册进 `ItemFactory`。

## 事件模型决策

> Canvas 自己的 `bind` / `tag_bind` / `current` / `focus` 逻辑由 C++ core
> 管理，不把所有东西塞回标准 Tk 事件。

基础窗口事件（FocusIn/FocusOut、Enter/Leave、TakeFocus、键盘遍历）仍需与
Tk 协调；这些走 `CanvasEventBridge`，通过 `Tk_CreateEventHandler` 安装。

### M6 的落地方式

- **事件模式匹配走 Tk 的 binding table**
- **事件类型与 Tk 的 canvas 对齐**
- 绑定脚本可以销毁画布：widget 的释放在事件派发结束后才进行
  （`CanvasWidget::BeginEvent` / `EndEvent`）。

## 命名说明

渲染实现与运行时行为均为 Direct2D/DirectWrite；构建链接 `d2d1.lib` /
`dwrite.lib`，不再使用 GDI+。

## Tk ⇄ Win32 集成（本项目最 tricky 的一层）

Tk/Win 上**每个 Tk widget 都拥有一个自己的 Win32 子窗口**（类名 `TkChild`），Tk 把该 widget 的背景画进这个窗口，并把 `WM_PAINT` 投递给它。因此画布不需要“自己造 HWND”，直接用 Tk 给它的那个：

- `CanvasWidget` 通过 `winfo id <path>` 取得自己的 HWND（`ToplevelPainter` →  现为 `WindowHook`），并 **subclass** 它的窗口过程。
- 绘制顺序：`WM_PAINT` → 先调用原窗口过程（Tk 画 widget 背景）→ 再用 `ID2D1DCRenderTarget` 绑定到 32bpp DIB section 离屏绘制 → `BitBlt` 回同一 DC。坐标是 widget 局部坐标，不需要屏幕/客户区换算。
- 事件掩码要用 **X11 event mask**（`ExposureMask|StructureNotifyMask|FocusChangeMask`），不是事件类型常量。
- `Expose` 事件里**不能**再 `Invalidate()`：窗口钩子本来就会在 Tk 绘制后画我们的内容，重复 invalidate 会造成无限重绘循环。
- `Tk_InitOptions` 必须在 `Tk_SetClass()` **之后**调用：Tk 通过 class 层级查选项数据库，class 未设置时会崩溃。

这样做的收益：Tk 的事件路由、焦点、层级顺序、裁剪全部照旧，只有像素归我们。

## 已知取舍（0.x）

- `create_window`（WindowItem）不支持，列入 M10。
- 图像：`drawImage` 已按 `ID2D1Bitmap*` 实现并处理 transform；Tk photo →
  D2D bitmap 的转换在 M9（`TkPhotoToRgba` 尚未实现）。
- D2D 单线程 factory（Tk 主线程）；跨线程访问需改 `FACTORY_TYPE_MULTI_THREADED`。
- 文字逐行测布局，行高 = `-font` 尺寸 ×1.25（或 `-linespacing` 覆盖，M9）。
