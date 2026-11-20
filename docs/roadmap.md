# 路线图（里程碑）

> 完整设计与风险分析见 [规划.md](规划.md) §12/§13/§15。此处跟踪落地进度。

## 阶段 0：环境验证（M0）✅ 已完成（2026-11-07）

- [x] Tcl/Tk 8.6 dev SDK 就位（`python tools/check_tcltk_dev.py` 通过 → 8.6.18）
- [x] `cmake --preset windows-release` 配置成功，产出 `tkd2dcanvas01.dll`
- [x] `package require tkd2dcanvas` 成功（返回 0.1，命令已注册）
- [x] Tcl 测试通过（Total 6 / Passed 6 / Failed 0）、Python 测试 6 passed

## 阶段 1：空 widget（M1 + M2）✅ 已完成（2026-11-07）

- [x] `tkd2dcanvas .c -width 400 -height 300` 创建 widget（真实 Tk 子窗口）
- [x] Tk window 显示、`configure`/`cget`、几何请求、销毁路径
- [x] 事件桩（Configure/Expose/Map/Destroy 经 Tk 事件掩码接入）
- [x] Direct2D 背景绘制（`WindowHook` 子类化 widget 自己的 HWND）

## 阶段 2：Direct2D 背景（M3）✅ 完成（2026-11-07）

- [x] `-background lightgray` 显示灰色背景（`-background` 走 Tk 选项表 + `winfo rgb`）
- [x] 双缓冲（32bpp DIB section + `BitBlt`）、`WM_PAINT` 正确、resize 不拉伸
- [x] 脏矩形裁剪（只重绘变化区域；随 M4 的 item 层一起落地：WM_PAINT 只重绘/BitBlt 受损矩形）

## 阶段 3：基本 item（M4）✅ 完成（2026-11-07）

- [x] rectangle / oval / line（含 `-fill`、`-outline`、`-width`、`-dash`）
- [x] item id、`create` 返回、重绘脏区
- [x] polygon / arc（`-fill`、`-outline`、`-width`、`-dash`；arc 另有 `-start`/`-extent`/`-style`）
- [x] `-joinstyle`（line/polygon 的拐角风格：bevel/miter/round，Tk 默认 round；其他 item 不开放）
- [x] `-capstyle`（line 的端点风格：butt/projecting/round，Tk 默认 butt；其他 item 不开放）
- [x] polygon `-smooth` / `-splinesteps`（样条平滑多边形；`-splinesteps` 控制曲线离散段数，Tk 默认 12，范围钳制 1–100；算法直译 Tk 源码 `TkMakeBezierCurve`，含 `-smooth raw` 三次贝塞尔 knot 模式）
- [x] line `-smooth` / `-splinesteps`
- [x] `-dashoffset`
- 备注：本项目不计划支持位图 stipple，故不实现该选项——设置 `-offset` 会被当作未知选项报错。`-outlineoffset`/line `-offset` 的 index 形式同理不提供。

## 阶段 4：基础命令（M5）✅ 完成（2026-11-07）

- [x] `coords` / `move` / `delete` / `itemconfigure` / `itemcget` / `bbox`
  - `coords` / `move` / `delete` / `bbox` 已随 M4 先落地（tag/id 并集解析、原子回滚；tag 解析按 z-order 排序）
  - `itemconfigure` / `itemconfig` / `itemcget` 已实现（`ItemStyle` 存储 + 两阶段解析/应用，全或无）
- [x] widget 级 `configure` / `cget`（M2 落地，M5 验收）

## 阶段 5：tag / find / bind（M6）✅ 已完成（2026-11-07）

- [x] `addtag` / `dtag` / `gettags`
- [x] `find`（`withtag` / `all` / `above` / `below` / `closest` / `overlapping` / `enclosed`）
- [x] `tag_bind`：C++ core 命中测试（`CanvasModel::FindItemAt` / `DistanceTo`）+ Tcl 脚本派发
  - 命中测试在 core：`CanvasItem::DistanceTo`（到路径的距离）+ `CanvasModel::FindItemAt`（顶层优先、容差 = `-closeenough`，默认 1.0）
  - 派发沿用 Tk 的 binding table
  - 事件类型与 Tk 的 canvas 一致：只接受 key/button/motion/enter/leave/virtual；虚拟事件可登记但永不触发
  - item 下的指针位置维护 `current` tag（与 Tk 一致）
- [x] `raise` / `lower`（z-order，成组移动）
- [x] 多重绑定与单独解绑

## 阶段 6：滚动（M7）✅ 完成（2026-11-07）

- [x] `xview` / `yview`（moveto/scroll/units/pages；语义直译 Tk `tkCanvas.c`）
- [x] `-xscrollcommand` / `-yscrollcommand` / `-scrollregion` / `-*scrollincrement`
  - `-scrollregion` 支持 `c`/`i`/`m`/`p` 单位
  - scroll command 按 Tk 的 UPDATE_SCROLLBARS 语义延迟到 idle 求值（`<prefix> <first> <last>`，TCL_EVAL_GLOBAL），错误走 bgerror
  - `-confine`/`-scrollregion`/`-*scrollincrement`/resize 都会对当前 origin 重新 round + confine（与 Tk 的 ConfigureNotify 一致）
  - 顺带实现 `scan mark` / `scan dragto ?gain?`（Tk canvas 滚动命令的其余部分）
  - 顺带修复了隐患：创建期 option 校验失败的 widget（如 `-scrollregion` 解析失败）没有走完整析构路径，pending idle 回调会指向已释放对象，统一改为 `AbortCreate`
- [x] 滚动帧增量重绘：`ScrollWindowEx` + 后备缓冲 `BitBlt` 位移，只重画新暴露条带；M10 落地时必须重新验证`SW_SCROLLCHILDREN` 与子窗口跟随
  - 残影修复：条带标记后立即 `UpdateWindow` 同步重绘，避免延迟 WM_PAINT 期间显示旧像素

## 阶段 7：Python 包装（M8）✅ 完成（2026-11-08）

- [x] wheel 可安装（`tkd2dcanvas-0.1.0-py3-none-win_amd64.whl`；干净 venv 验证）
- [x] `load_into`：幂等（`package present` 判断），不依赖 Python id 缓存；只接受  显式 master（传 `None` 抛 `LoadError`），缺pkgIndex.tcl 时报
  明确的 `LoadError`
- [x] `scale` / `canvasx` / `canvasy` / `type` / `focus`
- [x] 未实现的 tkinter.Canvas 方法显式抛 `NotImplementedError`（不依赖缺失的 Tcl 子命令）

## 阶段 8：图像与文字增强（M9）✅ 已完成（2026-11-08）

- [x] `create_text` 完整（font/anchor/justify/width/wrap）
- [x] `create_image`（Tk photo → D2D bitmap）
- [x] font 解析（`font actual`）、`create_bitmap`

## 阶段 9：Window item（M10）✅ 完成（2026-11-08）

- [x] `create_window`：真实 Tk 子 HWND 嵌入

## 阶段 10：打包与发布（M11）

- [ ] wheel（x64）、安装验证、文档、签名/校验

## 阶段 11：纯 GDI 后端（M12，分支 `feature/gdi-antialias`）

> 目标：验证"不用 D2D、只用 GDI（含原生 Tk 画布）"也能做到同等质量的边缘。
> 详见 [gdi_antialias.md](gdi_antialias.md)。

- [x] Tk 侧：在 `third_party/tcltk-src/tk` 上实现手动抗锯齿引擎
      （补丁 `third_party/patches/tk-8.6-gdi-antialias.patch`），
      `canvas -antialias 1 -aafactor 4`；Tk 自带 `canv*.test`
      395 例 → 309 通过 / 86 跳过 / **0 失败**
- [x] 仓库侧：`native/render/GdiAntialias.{h,cpp}` 超采样引擎 +
      `native/render/GdiRenderer.{h,cpp}` 渲染器，
      `-renderer gdi` / `-antialiasfactor N` 两个选项
- [x] 抗锯齿引擎单元测试（`tests/gdi_aa/aa_render_test.cpp`）
- [x] 边缘特性探针（`tests/gdi_aa/aa_edge_probe.cpp`）：
      裁剪安全性、覆盖率量化、相邻图元接缝、**分块渲染无损**
- [x] 像素对齐矩形的等价性测试（`tests/gdi_aa/aa_rect_equiv.cpp`）：
      轴对齐 + 整数坐标的矩形走 `FillRect` 快路径（与抗锯齿引擎逐像素相同）
- [x] 与 D2D 后端逐像素对比（普通 / 滚动场景）
- [x] 滚动路径：两个后端共用（后备缓冲 BitBlt 移位 + 只重画暴露的条带），
      保留区是纯移位、不重新渲染
- [x] **滚动时的内存增长**：离屏缓冲改为"按裁剪框裁剪 + 分块渲染"，
      大小只取决于预算（默认 1 MB），不再随图元变大。
      TinUI 演示里滚动 12 步的内存增长从 +9.1 MB 降到 +1.7 MB，画面逐像素不变
      （详见 [gdi_antialias.md](gdi_antialias.md) 8.10）
- [x] **同一片区域来回滚动时的增长**：内嵌窗口只在位置真的变了时才移动
      （与 Tk 的画布一致），并且每个事件循环只挪一次；滚动区这类不改变像素的
      选项不再触发整屏重画；抗锯齿引擎不再每个图元新建画笔画刷。
      `<Configure>` 的次数因此和 Tk 自带的画布完全一样，
      40 趟来回滚的内存增长从"12 趟 +9.1 MB"降到"+816 KB"
      （详见 [gdi_antialias.md](gdi_antialias.md) 8.11）
- [ ] 离屏缓冲改成进程级共享（现在是每个画布控件一份，每份最多 1 MB）
- [ ] d2d 后端自己的增长（渲染目标的资源释放是延迟的，40 趟来回滚 +30 MB）
- [ ] 覆盖率量化带来的边缘台阶（4 倍 17 级 vs D2D 解析 256 级）——
      需要更高倍数或解析覆盖率才能根治
