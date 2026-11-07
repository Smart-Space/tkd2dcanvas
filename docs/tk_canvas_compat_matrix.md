# tkinter.Canvas 兼容矩阵

图例：✅ 已实现　🚧 进行中　⬜ 计划中　❌ 明确不支持

> 0.1.0 为骨架期，所有功能均为 ⬜/🚧；此矩阵随里程碑更新。

| tkinter.Canvas API | 状态 | 备注 |
| --- | --- | --- |
| `create_rectangle` | ✅ (M4) | 首批 item：rect/oval/line |
| `create_oval` | ✅ (M4) | |
| `create_line` | ✅ (M4) | 支持多点折线；-fill 作画笔颜色；-joinstyle bevel/miter/round（默认 round）；-capstyle butt/projecting/round（默认 butt）；-smooth/-splinesteps（默认 0/12，Tk 算法直译，仅点数 > 2 生效） |
| `create_polygon` | ✅ (M4) | 默认黑色填充；支持多点折线；-joinstyle bevel/miter/round（默认 round）；-smooth/-splinesteps（默认 0/12；`-smooth raw` 三次贝塞尔 knot 模式） |
| `create_arc` | ✅ (M4) | 含 -start/-extent/-style（pieslice/chord/arc）；-offset 明确不支持|
| `create_text` | ⬜ (M4 基础 / M9 增强) | font/anchor/justify |
| `create_image` | ⬜ (M9) | 依赖 Tk photo |
| `create_bitmap` | ⬜ (M9) | XBM |
| `create_window` | ❌ 0.x | 见 roadmap M10 |
| `coords` | ✅ (M4/M5) | 支持 tag/id 并集；单个 list 参数亦可 |
| `move` | ✅ (M4/M5) | |
| `delete` | ✅ (M4/M5) | |
| `itemconfigure` / `itemconfig` | ✅ (M5) | 支持 tag/id 并集设置；无选项时报告 dict |
| `itemcget` | ✅ (M5) | 返回首项匹配值；颜色以 #rrggbb 返回 |
| `config` / `configure` | ✅ (M2/M5) | widget 级选项已实现、M5 验收 |
| `cget` | ⬜ (M5) | |
| `bbox` | ✅ (M4/M5) | 多 item 取并集；无匹配返回空串 |
| `find_withtag` / `find_all` / `find_closest` / `find_overlapping` / `find_enclosed` | ⬜ (M6) | |
| `addtag_withtag` / `addtag_all` / `addtag_overlapping` / `dtag` | ⬜ (M6) | |
| `gettags` | ⬜ (M6) | |
| `tag_bind` / `bind` | ⬜ (M6) | C++ core 解析绑定 |
| `tag_raise` / `tag_lower` | ⬜ (M6) | z-order |
| `xview` / `yview` | ⬜ (M7) | scrollregion + scrollcommand |
| `scan_mark` / `scan_dragto` | ⬜ (M7) | |
| `postscript` | ❌ | 打印路径不计划 |
| `create_a_window` 系别名 | ❌ | 不存在 |
| pack/grid/place 管理 | ✅ 托管 | 走 Tk geometry manager（示例已用） |
| `winfo_*` | ✅ 托管 | 由宿主 Tk 提供 |

## 选项支持情况（widget 级）

`-width -height -background/-bg -cursor -borderwidth/-bd -highlightthickness
-xscrollincrement -yscrollincrement -closeenough -confine -takefocus
-scrollregion -xscrollcommand -yscrollcommand`

均为解析层已识别（见 `native/tk_glue/CanvasOptions.cpp`），行为随 M2/M5 落地。

## 选项支持情况（item 级）

通用：`-fill -outline -width -dash -dashoffset -tags`。
line/polygon：`-joinstyle`；line 另有 `-capstyle`。
line/polygon 另有：`-smooth`（`0`/布尔/`true`/`bezier`/`raw`，支持 Tk 前缀缩写，空串 = 关闭）、`-splinesteps`（整数，钳制 1–100，默认 12）。
arc：`-start -extent -style`。
rectangle/oval 不开放 -smooth/-splinesteps。

明确不支持（会按未知选项报错）：
- `-offset`（arc/line/polygon 等的 stipple 偏移；Tk 该选项仅用于位图平铺，本项目不计划支持位图 stipple）
- `-outlineoffset`
- `-stipple` / `-outlinestipple` 及相关 `-activestipple` / `-disabledstipple`
- line/polygon 的 `-offset` index 形式

## 行为差异（相对 tkinter.Canvas）

- 命中测试容差 `-closeenough` 默认 2.0（与 Tk 一致）。
- 坐标单位与像素 1:1（DPI 缩放完善见 Win32Dpi，0.x 暂不做整体缩放）。
- 事件属性对象（`event.x` 等）：`tag_bind` 的 Python 回调签名由 Python 层
  适配（M6 完成后更新本文档）。
- Tk 的 stipple 相关选项均不支持，设置时按未知选项报错。
- `-dashoffset` 按 canvas 单位解析（Tk 的 `<length>` 单位词尾（`c`/`i`/`m`/`p`）不支持，按纯数字解析）。
- `-smooth` 平滑只改变绘制与命中测试用的离散曲线点；`coords` 仍返回原始顶点。
- 平滑多边形的 bbox 按原始顶点计算（Tk 行为：曲线不参与 bbox）。
