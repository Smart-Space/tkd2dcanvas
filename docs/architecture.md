# 架构说明

> 原始规划见 [规划.md](规划.md)。
> **当前实现**：渲染后端有两条路径 —— **Direct2D / DirectWrite**（默认，
> `native/render/D2DRenderer.cpp`）和 **纯 GDI + 手动抗锯齿**
> （`native/render/GdiRenderer.cpp`，见 [gdi_antialias.md](gdi_antialias.md)）。
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
    │                     ^
    │                     └── Renderer 接口（native/render/Renderer.h）
    v
渲染器：D2DRenderer（默认） 或 GdiRenderer（-renderer gdi）
```

## 分层与依赖方向

| 层 | 目录 | 依赖 |
| --- | --- | --- |
| 核心模型 | `native/include` + `native/core` | 无 |
| Item 实现 | `native/items` | core |
| 渲染抽象/实现 | `native/render` | core、d2d1/dwrite、gdi32 |
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
  `SurfaceRenderer`（`native/render/SurfaceRenderer.h`）在它之上加了两个
  "绑定到 DC" 的方法（`AttachToHdc` / `Detach`），让 `Win32Surface` 能把
  后备缓冲的 HDC 交给渲染器。
  两个实现：`D2DRenderer`（默认）和 `GdiRenderer`（`-renderer gdi`）。
- `D2DRenderer`：
  - 绑定内存 HDC：`ID2D1DCRenderTarget` + 32bpp top-down DIB section
    （`Win32DoubleBuffer` 按此实现）→ 渲染完成后 `BitBlt` 到目标 DC，无闪烁。
  - 平滑曲线：`-smooth` 走 `SmoothCurve`（`native/core`，直译 Tk 的`TkMakeBezierCurve`/`TkMakeRawCurve`）预先展开成折线再交给 D2D。
  - 虚线：`PenCache` 缓存 `ID2D1StrokeStyle`；D2D 的 dash 数组与dashOffset 均以 stroke width 为单位，`PenCache` 把 Tk 的绝对canvas 单位（-dash / -dashoffset）除以线宽后传入。
  - 文字：DirectWrite（`IDWriteTextLayout`，按行绘制，支持bold/italic/underline/strikethrough、Tk anchor 九宫格定位与 `-angle` 旋转）；
  - 裁剪：`SetAxisAlignedClip` / `ResetAxisAlignedClip`，支持嵌套 push/pop
    （`pushClip` 取交集，`popClip` 恢复）。
  - 抗锯齿：`SetAntialiasMode(PER_PRIMITIVE)`，文字 ClearType。
  - 断设备（`D2DERR_RECREATE_TARGET`）：自动丢弃 render target，下一帧重建。
- `GdiRenderer`：纯 GDI 后端，边缘的抗锯齿由本仓库自己的超采样引擎
  （`native/render/GdiAntialias.cpp`）算——GDI 本身没有抗锯齿。
  画图元只用到 `gdi32`（外加 `msimg32` 的 `AlphaBlend`）。
  - 用法：`-renderer gdi`，可选 `-antialiasfactor N`（超采样倍数，2…8，默认 4）。
  - 只能在**创建时**选择后端（见 [gdi_antialias.md](gdi_antialias.md) §8.9）。
  - 细节、限制与实测数据见 [gdi_antialias.md](gdi_antialias.md) §8。
- `Win32Surface`：32bpp 双缓冲 + D2D 绘制 + `BitBlt`；由 Tk 的Expose/Configure 事件驱动 `Paint()`。`WM_PAINT` 使用精确的 update region 矩形列表（不是包围盒）；滚动时先用 `ScrollWindowEx` + 后备缓冲`ScrollContent` 平移像素，再只重画新暴露条带；滚动命令内还会用 `UpdateWindow` 强制同步绘制，避免条带在延迟绘制前显示旧内容。`ScrollWindowEx` 不再移动子窗口（`SW_SCROLLCHILDREN` 已移除）：window item 的内嵌窗口由 `SyncWindowItems`（tk_glue/CanvasTkWindowItem.cpp）按模型 bbox − view origin 显式定位/map/unmap，分数 origin 与完整 repaint fallback 路径都能对齐；子窗口移动后会使其旧区域重绘。
- **进程级共享的离屏资源**（照 Tk 的 `tkWinAA.c`：那里的抗锯齿引擎把离屏缓冲
  放在所有画布共享的状态里）：
  - `GdiScratchPool`（`native/render/GdiScratchPool.{h,cpp}`）：抗锯齿引擎的
    超采样暂存缓冲 + 回读缓冲 + 画刷 + 画笔缓存，**整个进程一份**
    （默认预算 1 MB），不再每个画布控件一份；临界区保证同一时刻只有一个
    形状在用这些缓冲。
  - `D2DTargetPool`（`native/render/D2DTargetPool.{h,cpp}`）：`ID2D1DCRenderTarget`
    和画笔**整个进程一个**（按光栅化器分硬件/软件两个池）；控件绘制前绑定到
    自己的后备缓冲，画完归还。控件换了后备缓冲只重新绑定、不重建；绑定带
    控件编号，控件没了绑定就作废。
  - 详见 [gdi_antialias.md](gdi_antialias.md) §8.12。
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

渲染实现与运行时行为默认为 Direct2D/DirectWrite；构建链接 `d2d1.lib` /
`dwrite.lib`，不使用 GDI+。`-renderer gdi` 额外链接 `gdi32.lib` /
`msimg32.lib`（`AlphaBlend` 在 `msimg32` 里），但那是同一次构建、同一份
DLL——两个后端共存于 `tkd2dcanvas01.dll`。

## Tk ⇄ Win32 集成（本项目最 tricky 的一层）

Tk/Win 上**每个 Tk widget 都拥有一个自己的 Win32 子窗口**（类名 `TkChild`），Tk 把该 widget 的背景画进这个窗口，并把 `WM_PAINT` 投递给它。因此画布不需要“自己造 HWND”，直接用 Tk 给它的那个：

- `CanvasWidget` 通过 `winfo id <path>` 取得自己的 HWND（`ToplevelPainter` →  现为 `WindowHook`），并 **subclass** 它的窗口过程。
- 绘制顺序：`WM_PAINT` → 先调用原窗口过程（Tk 画 widget 背景）→ 再用 `ID2D1DCRenderTarget` 绑定到 32bpp DIB section 离屏绘制 → `BitBlt` 回同一 DC。坐标是 widget 局部坐标，不需要屏幕/客户区换算。
- 事件掩码要用 **X11 event mask**（`ExposureMask|StructureNotifyMask|FocusChangeMask`），不是事件类型常量。
- `Expose` 事件里**不能**再 `Invalidate()`：窗口钩子本来就会在 Tk 绘制后画我们的内容，重复 invalidate 会造成无限重绘循环。
- `Tk_InitOptions` 必须在 `Tk_SetClass()` **之后**调用：Tk 通过 class 层级查选项数据库，class 未设置时会崩溃。

这样做的收益：Tk 的事件路由、焦点、层级顺序、裁剪全部照旧，只有像素归我们。

## 已知取舍（0.x）

- 图像：bitmap 按 item id+generation 缓存，上限 64 项）。photo 在配置时拷贝，photo 后续内容变更不自动反映到 canvas。
- D2D 单线程 factory（Tk 主线程）；跨线程访问需改 `FACTORY_TYPE_MULTI_THREADED`。
