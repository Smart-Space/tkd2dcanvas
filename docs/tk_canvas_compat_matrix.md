# tkinter.Canvas 兼容矩阵

图例：✅ 已实现　🚧 进行中　⬜ 计划中　❌ 明确不支持

> 0.1.0 为骨架期，所有功能均为 ⬜/🚧；此矩阵随里程碑更新。

| tkinter.Canvas API | 状态 | 备注 |
| --- | --- | --- |
| `create_rectangle` | ✅ (M4) | 首批 item：rect/oval/line |
| `create_oval` | ✅ (M4) | |
| `create_line` | ✅ (M4) | 支持多点折线；-fill 作画笔颜色；-joinstyle bevel/miter/round（默认 round）；-capstyle butt/projecting/round（默认 butt）；-smooth/-splinesteps（默认 0/12，Tk 算法直译，仅点数 > 2 生效） |
| `create_polygon` | ✅ (M4) | 默认黑色填充；支持多点折线；-joinstyle bevel/miter/round（默认 round）；-smooth/-splinesteps（默认 0/12；`-smooth raw` 三次贝塞尔 knot 模式） |
| `create_arc` | ✅ (M4) | 含 -start/-extent/-style（pieslice/chord/arc）；-offset 明确不支持；`-start`/`-extent` 在配置时按 Tk 归一化（start 归一到 [0, 360)，extent 按整圈截掉；`-extent 360` 归一为 0） |
| `create_text` | ✅ (M9) | `-text`/`-font`/`-fill`/`-anchor`/`-justify`/`-width`（换行宽度，0=不换行）/`-wrap`（word/char/none）/`-angle`（逆时针，[0, 360)） |
| `create_image` | ✅ (M9) | Tk photo → RGBA 拷贝（Straight→Premultiplied）；`-image`/`-anchor`；photo 在配置时拷贝像素，之后 photo 内容变更不会自动反映到 canvas |
| `create_bitmap` | ✅ (M9) | 内嵌 Tk 预定义 XBM（error/gray12/gray25/gray50/gray75/hourglass/info/questhead/question/warning）+ `@file.xbm`；`-bitmap`/`-foreground`/`-background`（`""`=透明/停用）/`-anchor` |
| `create_window` | ✅ (M10) | 真实 Tk 子窗口嵌入；`-window`（必须存在，必须是 canvas 或其祖先的子窗口；toplevel/canvas 自身报错）、`-width`/`-height`（像素，0=内嵌窗口自然尺寸）、`-anchor`（默认 center）、`-state`；内嵌窗口 requested size 变化自动回流 bbox（几何管理器）；bbox 与滚动一致（`SyncWindowItems` 显式定位，见 roadmap M10）；窗口项恒绘制在图形项之上；相对次序用 Tk `raise`/`lower` 调整；`itemconfigure -window ""` 可解除嵌入 |
| `coords` | ✅ (M4/M5) | 支持 tag/id 并集；单个 list 参数亦可 |
| `move` | ✅ (M4/M5) | |
| `moveto` | ✅ (M6) | 语义同 Tk：把**第一个匹配 item** 的 bbox 左上角对齐到 (x, y)，所有匹配 item 按同一增量平移 |
| `delete` | ✅ (M4/M5) | |
| `itemconfigure` / `itemconfig` | ✅ (M5/M8) | 支持 tag/id 并集设置；无选项时报告 **dict**（`{选项名: 当前值}`，键不带 `-`），不是 tkinter 的 5 元组；item 不匹配时返回 `{}` |
| `itemcget` | ✅ (M5) | 返回首项匹配值；颜色以 #rrggbb 返回 |
| `config` / `configure` | ✅ (M2/M5/M8) | widget 级选项已实现、M5 验收；无参时报告 **dict**（`{选项名: 当前值}`，键不带 `-`）；设置时返回 `None`（同 tkinter）；`keys()` 返回 dict 报告的键 |
| `cget` | ✅ (M5) | 读取 widget 级选项 |
| `bbox` | ✅ (M4/M5) | 多 item 取并集；无匹配返回空串 |
| `find_withtag` / `find_all` / `find_closest` / `find_overlapping` / `find_enclosed` | ✅ (M6/M8) | `find` 支持 withtag/all/above/below/closest/overlapping/enclosed；`find_closest(x, y, halo, start)` 支持 `start`；Tk 4 拼写 `findwithtag`/`findall`/`findclosest` 亦可用；Python 层 `find*` / `find_above` / `find_below` 返回 **tuple**（与 tkinter 一致） |
| `addtag_withtag` / `addtag_all` / `addtag_above` / `addtag_below` / `addtag_closest` / `addtag_overlapping` / `addtag_enclosed` / `dtag` | ✅ (M6/M8) | `addtag` 支持全部检索命令（含 `above`/`below`/`closest [halo] [start]`）；`dtag` 支持 `dtag item tag` 与 `dtag tag` 两种写法 |
| `gettags` | ✅ (M6/M8) | 按添加顺序返回；Python 层返回 **tuple**（与 tkinter 一致） |
| `tag_bind` / `bind` | ✅ (M6) | C++ core 解析绑定：命中测试在 core，事件模式匹配走 Tk binding table；事件类型与 Tk 的 canvas 一致（key/button/motion/enter/leave/virtual）；支持 `add=True` 多重绑定，返回回调 id，可单独 `unbind` |
| `tag_raise` / `tag_lower` | ✅ (M6) | `raise`/`lower` 成组移动，保持相对顺序 |
| `xview` / `yview` | ✅ (M7) | `xview` / `xview moveto f` / `xview scroll n units\|pages`；fractions 语义与 Tk 的 `ScrollFractions` 一致（无 region 时恒为 `0.0 1.0`）；moveto 映射到**整个 region 宽度**再由 confine 拉回（Tk 行为）；`-xscrollincrement`>0 时 origin 四舍五入到 increment 的倍数 |
| `scan_mark` / `scan_dragto` | ✅ (M7) | `scan mark x y` 记录位置与当时的 origin；`scan dragto x y ?gain?` 按 gain（默认 10）倍距离反向拖动 origin（Tk 语义，origin 取整 + confine） |
| `postscript` | ❌ | 打印路径不计划 |
| `scale` | ✅ (M8) | 原生 `Scale` 实现（item 坐标按点变换，rectangle/oval/arc 存储保持归一化）；负系数镜像 |
| `canvasx` / `canvasy` | ✅ (M8) | 原生 `canvasx`/`canvasy`（window 坐标 + view 原点；`gridspacing`>0 时按 Tk 取整到最近倍数，Math"round half away"） |
| `type` | ✅ (M8) | 原生 `type`（首个匹配 item；Python 层无匹配返回 `None`，同 tkinter） |
| `focus` | ✅ (M8) | 原生 `focus`：仅支持插入光标的 item（目前无）可被设为焦点，与 Tk 一致；`focus` 无参返回焦点 item id（Python 层 `int`/`None`）；键盘事件只派发给焦点 item |
| `create_a_window` 系别名 | ❌ | 不存在 |
| `select_*`（select_adjust/from/to/clear/item） | ❌ | selection item 未实现 |
| `dchars` / `icursor` / `insert` / `index` | ❌ | 文字 item 编辑命令未实现（插入光标/选区路径未做） |
| `keys()` | ✅ (M8) | 返回 dict 报告的键（不带 `-`），与 tkinter 一致 |
| pack/grid/place 管理 | ✅ 托管 | 走 Tk geometry manager（示例已用）；返回 `None`（同 tkinter） |
| `winfo_*` | ✅ 托管 | 由宿主 Tk 提供 |

## 选项支持情况（widget 级）

`-width -height -background/-bg -cursor -borderwidth/-bd -highlightthickness
-highlightbackground -highlightcolor -insertbackground -insertborderwidth
-insertwidth -selectbackground -selectborderwidth -selectforeground
-xscrollincrement -yscrollincrement -closeenough -confine -takefocus
-scrollregion -xscrollcommand -yscrollcommand`

## 选项支持情况（item 级）

通用：`-fill -outline -width -dash -dashoffset -tags -state`。
带 `-fill` 的 item（text 及所有 shape）另有：`-disabledfill`。
line/polygon：`-joinstyle`；line 另有 `-capstyle`。
line/polygon 另有：`-smooth`（`0`/布尔/`true`/`bezier`/`raw`，支持 Tk 前缀缩写，空串 = 关闭）、`-splinesteps`（整数，钳制 1–100，默认 12）。
arc：`-start -extent -style`。
rectangle/oval 不开放 -smooth/-splinesteps。
window item 专用：`-window`（内嵌窗口 path）、`-height`（像素，0=自然尺寸）、`-anchor`（nw/n/ne/e/se/s/sw/w/center，默认 center）；`-width` 在 window item 上同为像素语义（`0`=内嵌窗口自然尺寸）。
text/image/bitmap/window item 均支持 `-anchor`（window 默认 center）。
明确不支持（会按未知选项报错）：
- `-offset`（arc/line/polygon 等的 stipple 偏移；Tk 该选项仅用于位图平铺，本项目不计划支持位图 stipple）
- `-outlineoffset`
- `-stipple` / `-outlinestipple` 及相关 `-activestipple` / `-disabledstipple`
- line/polygon 的 `-offset` index 形式

## 行为差异（相对 tkinter.Canvas）

### M8（Python 包装）

- `itemconfigure(item, "width")` / `configure("width")` 返回单个选项的当前值（tkinter 返回 5 元组）；未知选项抛 `CommandError`。
- `load_into(master=None)` 抛 `LoadError`（必须传入已存在的 root/widget，不会隐式创建 root）；Tcl 侧以 `package require` 幂等加载，无 Python侧缓存。

### 其余既有差异

- 命中测试容差 `-closeenough` 默认 **1.0**
- 坐标单位与像素 1:1（DPI 缩放完善见 Win32Dpi，0.x 暂不做整体缩放）。
- 事件属性对象（`event.x` 等）：`tag_bind` 的 Python 回调签名由 Python 层
  适配（M6 完成后更新本文档）。
- Tk 的 stipple 相关选项均不支持，设置时按未知选项报错。
- `-dashoffset` 按 canvas 单位解析（Tk 的 `<length>` 单位词尾（`c`/`i`/`m`/`p`）不支持，按纯数字解析）。
- `-smooth` 平滑只改变绘制与命中测试用的离散曲线点；`coords` 仍返回原始顶点。
- 平滑多边形的 bbox 按原始顶点计算（Tk 行为：曲线不参与 bbox）。
- `coords` 输出为最简形式（`"102 102 …"`），Tk 输出带小数点（`"102.0 102.0 …"`），数值相等；`bbox` 输出恒为整数（与 Tk 一致）。

### 描边的像素对齐（Direct2D 渲染器）

Tk 的画布用 GDI **无抗锯齿**地描边：描边覆盖整像素，落在半像素上的边缘按像素中心采样决定归属。Direct2D 默认带抗锯齿，同一条边缘会被摊到两行像素上（各 50%覆盖），于是 1px 的边框看起来是 2px 宽、两侧发虚——典型例子是 TinUI 的圆角按钮：「9px 描边的外框多边形 + 内缩 1px 的 9px 描边填充多边形」叠出来的 1px 边框。

渲染器在描边边缘落在半像素上时，把**描边几何**整体偏移半像素，使描边边缘重新落在整数像素上（覆盖度变成 0/1，边框变成 1 行实心像素，与 Tk 一致；曲线与斜边仍保持抗锯齿）。偏移**按轴分别决定**：只有当该轴上所有描边边缘都处于同一个半像素相位时才会偏移，否则宁可不动（避免把图形拉变形）：

- 闭合路径（矩形、轴对齐多边形）：该轴上各顶点坐标 ± 半宽所成的边缘集合相位一致；
- 开放线段：垂直于线段的一轴取线段所在坐标 ± 半宽；沿线段方向的一轴取**端帽尖点**——圆头/方头的尖点在端点外侧半宽处，平头的端点就是端点本身。

沿线段方向也做偏移，是为了**相互对齐的图元用同一套偏移**。TinUI 的输入框用两个嵌套多边形画边框，底部提示线是独立的 `line` 图元；提示线若不沿线段方向偏移，就会比边框偏半个像素，两端的圆头切法也不一样，看上去不对称。

由此产生的可见差异：

- 闭合图形的**描边相对填充偏移 0.5px**：1px 描边从「骑在路径上」变成「落在路径内侧」，整体轮廓与 Tk 相差半个像素。
- 路径含斜边/曲线、描边宽度为小数、或该轴上各边缘相位不一致（例如端点落在不同相位上的滚动条滑块）时，不做偏移，边缘仍按抗锯齿绘制（此时与 Tk 的差异就是 Tk 本身的抗锯齿差异）。
- **只有真正的端帽沿线段方向偏移**：两点多边形（Tk 的多边形没有 `-capstyle` 选项）在画布上是一段带**圆角接头**的线，接头不是端帽，因此不会被沿线段方向移动——滚动条滑块这类细线的圆头形状与之前完全一致。
- 命中测试与 bbox 仍按未偏移的路径计算，与绘制的描边相差 0.5px（可忽略）。


### M6（tag / find / bind）的具体差异

命中测试规则（与 Tk 逐点比对确认一致）：

- 判定量是「点到 item **路径**（描边中心线）的距离」，再减去半个描边宽度
  （描边覆盖路径两侧各半个宽度）；结果 ≤ `-closeenough` 即命中。
- **有填充**的形状内部整体命中；**无填充**的形状只有靠近描边处才命中
  （矩形/椭圆）。多边形例外：只要点在多边形内部就算命中（与填充无关，
  与 Tk 的 `TkPolygonToPoint` 一致）。
- `find closest` 用的是**未减去描边宽度**的路径距离（与 Tk 的 `ItemPoint` 一致），
  `halo` 先从距离里减掉、最低为 0；距离并列时顶层 item 优先。
  `find closest` 只返回**一个** item。
- `find above` / `find below` 与 Tk 一致，只返回紧邻的一个 item。
- `find overlapping` / `find enclosed`：**line / rectangle / oval / polygon / arc** 均用 Tk 的精确 areaProc 与 Tk 一致；先以 ±1px 整数化 bbox 粗筛再精确判定（与 Tk 的 FindArea 一致）。
- tag 绑定的派发顺序：`all` → item 的 tag（**按添加顺序**）→ item 本身；
  与 Tk 的 canvas 完全一致。指针不在任何 item 上时**不派发**（`all` 也不例外），
  但键盘事件例外：没有当前 item 时只派发 `all`。
- item 下的指针位置会带上 `current` tag（Tk 行为）。
- 键盘事件只派发给**焦点 item**（`.c focus` 设置，Tk 语义；此前版本
  走「当前 item」，已在 M8 修正）；没有焦点 item 时**不派发**到任何
  item（widget 级 `bind` 由宿主 Tk 处理，不受影响）。
- `bind` 支持 tag 与 item id；item id 的绑定在该 item 被删除后自动失效
  （item id 不复用，因此不会串到别的 item 上）。
- 多重绑定：脚本以 `+` 开头时**追加**到已有绑定之后（Tk 的语义），按注册顺序
  依次执行；不带 `+` 则替换整个绑定。删除一个不存在的绑定不报错。
- tag 绑定只接受 Tk 的 canvas 允许的事件类型（key / button / motion /
  enter / leave / virtual），其余（如 `<FocusIn>`）按 Tk 的原文报错：
  `requested illegal events; only key, button, motion, enter, leave, and
  virtual events may be used`。
- 虚拟事件（`<<Foo>>`）的 tag 绑定与 Tk 一致：**可以被登记，但永远不会触发**
  （Tk 把虚拟事件投递给窗口，而不是画布的 tag）。
- Python 层：`tag_bind` / `bind` 传入回调时返回该回调的 id；
  `tag_unbind(tag, sequence, funcid)` / `unbind(sequence, funcid)` 只移除这一个
  回调（同一 sequence 的其它回调保留），不传 `funcid` 则清掉整个绑定——
  与 tkinter 的 `_unbind` 行为一致。
