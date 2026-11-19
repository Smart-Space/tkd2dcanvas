# 用纯 GDI 实现手动抗锯齿（Manual AA on top of GDI）

> 分支：`feature/gdi-antialias`
>
> 目标平台：Windows（`third_party/tcltk-src/tk`，Tk 8.6.18）

## 1. 问题：GDI 能做抗锯齿吗？

**能，但只能"手动"做。**

GDI 的光栅化器本身是**非抗锯齿**的（aliased）：所有路径（直线、椭圆、多边形）
都按像素网格硬边光栅化，一条斜线的边缘就是一级级的台阶。GDI 也没有任何
"把这条路径平滑一下"的开关——`SetROP2`、`SetGraphicsMode` 之类的都改不了这件事。
GDI+ / Direct2D / DirectWrite 提供的抗锯齿是**另一个光栅化器**的功劳。

不过 GDI 提供了绘制任意形状的全部原语（`Ellipse`、`Polygon`、`Polyline`、
`Arc`、`BitBlt`……），而"超采样 + 盒式滤波降采样"这件事**完全可以用 GDI 的原语
拼出来**：

```text
 1. 用普通的 GDI 调用，把形状以 N 倍分辨率画进一张离屏 DIB
        ↓  （N 倍分辨率下，台阶细了 N 倍）
 2. 把这张离屏 DIB 用盒式滤波（box filter）降采样回设备分辨率
        ↓  （每个设备像素得到它被形状覆盖的面积比例 = 覆盖率）
 3. 把覆盖率当作 alpha，把形状合成到目标上
```

第 2 步得到的覆盖率就是**精确的面积覆盖率**，把它当作 alpha 合成，就是教科书
意义上的抗锯齿。整个过程只用到 GDI（`gdi32`），没有 GDI+、没有 Direct2D、
没有 DirectWrite，也没有任何第三方代码。

> 顺带说明：这就是 GDI+ 的 `SmoothingMode` 在"抗锯齿关闭"时的实现方式，
> 也是老版本 DirectDraw "smooth" 模式的实现方式。

## 2. 实现

### 2.1 代码位置

| 文件 | 作用 |
| --- | --- |
| `tk/win/tkWinAA.h` | 抗锯齿引擎的对外接口 |
| `tk/win/tkWinAA.c` | 抗锯齿引擎：离屏缓冲、降采样、合成 |
| `tk/win/tkWinDraw.c` | 把 `XFillArc` / `XDrawArc` / `XFillPolygon` / `XDrawLines` 接到引擎上 |
| `tk/generic/tkCanvas.c` | 画布的 `-antialias` / `-aafactor` 选项，以及重绘时开启/关闭抗锯齿 |
| `tk/generic/tkCanvas.h` | `TkCanvas` 结构里新增的两个字段，以及 Windows 专用钩子声明 |

### 2.2 数据流

```text
DisplayCanvas (tkCanvas.c)
   │
   ├─ 建立画布的离屏 pixmap，用背景色清空
   │
   ├─ TkWinCanvasAAEnter(canvas)          ← 打开抗锯齿开关（仅当 -antialias 1）
   │
   ├─ 逐个 item 调用 ItemDisplay
   │      │
   │      └─ XFillArc / XDrawArc / XFillPolygon / XDrawLines   (tkWinDraw.c)
   │             │
   │             ├─ AAUsable(gc)?          ← 判断该图形上下文能否抗锯齿
   │             │
   │             └─ AARenderArc / AARenderObject
   │                    │
   │                    ├─ TkWinAABegin()    ← 准备 N 倍分辨率的离屏 DIB（清零）
   │                    ├─ 用 N 倍坐标调用普通 GDI 原语，把形状画成"掩膜色"
   │                    └─ TkWinAAEnd()      ← 降采样求覆盖率 → 与目标混合 → 写回
   │
   ├─ TkWinCanvasAALeave()                ← 关闭抗锯齿开关
   │
   └─ 把画布 pixmap 拷贝到窗口
```

### 2.3 掩膜技巧

降采样时需要知道"哪些采样点被形状覆盖了"。引擎的做法是：

1. 把离屏 DIB 清成 0（黑）；
2. 让形状用一个**特殊的掩膜色** `RGB(1,2,3)` 画进去；
3. 采样点只要等于掩膜色，就算"被覆盖"。

之所以要专门用一个掩膜色，而不是直接用形状本来的颜色，是因为降采样时需要把
"覆盖率"和"颜色"分开：覆盖率来自掩膜，颜色来自 GC 的 `foreground`。

### 2.4 一个关键的字节序问题

`COLORREF`（GDI 的颜色，`RGB(r,g,b) = r | g<<8 | b<<16`，即 `0x00BBGGRR`）
和从 32 位 DIB 里读出来的 `DWORD`（内存里是 B,G,R,X，即 `0xAARRGGBB`）
**字节序是相反的**。直接比较会永远不相等，形状就会"画不出来"。
引擎里用 `AA_COLOR_TO_DWORD()` 做转换：

```c
#define AA_COLOR_TO_DWORD(c) \
    ((DWORD) (((c) >> 16) & 0xFF) \
	    | ((DWORD) ((c) & 0xFF00)) \
	    | ((DWORD) ((c) & 0xFF) << 16))
```

## 3. 用法

```tcl
canvas .c -width 400 -height 300 -background white \
    -antialias 1 -aafactor 4
```

| 选项 | 默认值 | 说明 |
| --- | --- | --- |
| `-antialias` | `0` | 是否对画布里的图形做手动抗锯齿 |
| `-aafactor` | `4` | 超采样倍数 N，每个设备像素采样 N×N 次（有效范围 2…8） |

示例见 `tests/gdi_aa/aa_demo.tcl`：同一个场景画两遍，左边是原生 GDI（锯齿），
右边是手动抗锯齿。

## 4. 边界与已知限制

* **文字不受影响**：GDI 自己就会对 TrueType 字形做灰度抗锯齿，不需要再处理。
* **虚线（`-dash`）不受影响**：GDI 的虚线样式长度是设备像素固定的，不会随
  超采样一起放大，放到 N 倍缓冲里会变得过密，因此虚线回退到普通绘制。
* **点画填充（stipple）不受影响**：同上，点阵图案不会被缩放。
* **非 `GXcopy` 的逻辑运算不受影响**：抗锯齿是"把形状按覆盖率混合到目标上"，
  只有拷贝语义才有意义。
* **超大图形**：超过 4096 像素或采样点超过 16M 的图形会回退到普通绘制，以免
  离屏缓冲失控。
* **性能**：每个图形需要一次 N 倍分辨率的离屏绘制 + 一次降采样 + 两次 `BitBlt`
  （读回目标、混合、写回），比普通绘制慢一个数量级以上。本机实测（半径 40 的圆，
  4 倍超采样，单线程）：

  | 单个形状的绘制耗时 | |
  | --- | --- |
  | 原生 GDI | 0.005 ms |
  | 手动抗锯齿 | 0.121 ms（约 24 倍） |

  抗锯齿是按画布开关的，只在需要时开启；画布静态内容会被 Tk 自己缓存，
  所以只有重绘时才付这份开销。

## 5. 构建与验证

```powershell
# 1. 把补丁应用到 Tcl/Tk 源码（或者直接用 tools\build_tk_aa.ps1）
git -C third_party/tcltk-src/tk apply ..\..\third_party\patches\tk-8.6-gdi-antialias.patch

# 2. 重新编译 Tk
cd third_party/tcltk-src/tk/win
nmake -f makefile.vc

# 3. 引擎的独立测试（不需要窗口，检查覆盖率是否正确）
pwsh -File tests\gdi_aa\build_engine_test.ps1 -Run

# 4. 画布上的对比演示 + 截图
pwsh -File tests\gdi_aa\run_demo.ps1 -Out aa_demo.png
```

`tests/gdi_aa/aa_engine_test.c` 会把一个圆画进离屏位图，然后统计
"完全覆盖 / 部分覆盖 / 完全没覆盖"的像素数，并打印边缘的覆盖率字符画，
用来确认覆盖率确实是 0…255 之间连续变化的。

回归检查用 Tk 自带的测试套件（改动的 `tkCanvas.c` / `tkWinDraw.c` 属于 Tk 本体）：

```powershell
cd third_party/tcltk-src/tk/tests
wish86t.exe all.tcl -file {canv*.test}
```

当前结果：395 个用例，309 通过，86 跳过（字体、图片类型等环境约束），0 失败。

## 6. 圆角：不需要新的图元类型

Tk 的 canvas 虽然没有 `roundedrectangle` 图元，但 **polygon 自带 `-joinstyle`
（8.6 里默认值就是 `round`）和 `-width`**，所以圆角有两种做法：

| 做法 | 写法 | 说明 |
| --- | --- | --- |
| 粗描边 + 圆角接头 | `create polygon {x1 y1 x2 y1 x2 y2 x1 y2} -fill "" -outline c -width 12 -joinstyle round` | GDI 把描边的**外角**倒圆，得到一条圆角的边框 |
| 填充圆角矩形 | 让 polygon 的**顶点落在圆弧上**（采样圆弧，见 `aa_demo.tcl` 的 `roundedRectPoints`） | 填充跟着圆角走；否则填充仍是直角，圆角接头只作用在描边上 |

要注意：第二种做法里，如果只是给直角矩形加 `-joinstyle round`，**填充部分仍是直角**，
而描边的圆角接头只覆盖外侧，内侧会留下一个小缺口——所以填充圆角矩形必须自己把
圆弧采样成顶点。

这两种做法在原生 GDI 下边缘都是锯齿的（演示图左半边），开了抗锯齿之后才是真正
平滑的（演示图右半边）。换句话说：**圆角本身用 polygon 就能凑出来，抗锯齿才是
让圆角"好看"的那一半**。

## 7. 与本仓库（Direct2D 版）的关系

本仓库的 `tkd2dcanvas` 用的是 Direct2D/DirectWrite，抗锯齿是渲染器自带的
（`SetAntialiasMode(PER_PRIMITIVE)`），不需要手工计算。

这条分支的目的是验证另一条技术路线：**如果不用 D2D，只用 GDI（含原生 Tk
画布），能不能做到同等质量的边缘**。结论是可以，代价是 CPU 侧的降采样开销，
以及上面列出的那几项限制。

对于"圆角"这类需求，两条路线是互补的：有了抗锯齿，圆角、斜边、圆形才不会出现
锯齿；而 `roundedrectangle` 之类的圆角图元本身仍然需要单独实现（原生 Tk 画布
没有这个图元类型）。

## 8. 仓库内的 GDI 后端（不用改 Tk 源码）

上面的实现是给**原生 Tk 画布**用的（要改 Tk 源码）。本仓库自己的
`tkd2dcanvas` 画布同样可以有一个纯 GDI 的后端：渲染器是可替换的，
`native/render/` 下多了一个实现，就能在**不碰 Tk 源码**的情况下用上同一套
手动抗锯齿。

### 8.1 代码位置

| 文件 | 作用 |
| --- | --- |
| `native/render/GdiAntialias.{h,cpp}` | 抗锯齿引擎：超采样、降采样、`AlphaBlend` 合成 |
| `native/render/GdiRenderer.{h,cpp}` | GDI 版渲染器（路径、文字、图片、裁剪、平移） |
| `native/render/SurfaceRenderer.h` | `Renderer` 接口加上"绑定到 DC"的两个方法 |
| `native/platform_win/Win32Surface.{h,cpp}` | 按 `-renderer` 选择渲染器 |
| `native/tk_glue/CanvasOptions.cpp` | `-renderer` / `-antialiasfactor` 两个选项 |

### 8.2 用法

```python
from tkd2dcanvas import Canvas

canvas = Canvas(root, width=400, height=300, renderer="gdi")
```

或在 Tcl 里：

```tcl
tkd2dcanvas .c -width 400 -height 300 -renderer gdi -antialiasfactor 4
```

| 选项 | 默认值 | 说明 |
| --- | --- | --- |
| `-renderer` | `d2d` | `d2d`（Direct2D）或 `gdi`（纯 GDI + 手动抗锯齿） |
| `-antialiasfactor` | `4` | 超采样倍数 N（有效范围 2…8） |

`-renderer` **只能在创建时指定**（运行时改它不会切换后端，见 8.8）。

### 8.2.1 构建产物在哪

GDI 后端和 D2D 后端在**同一个 DLL** 里（`tkd2dcanvas01.dll`），靠
`-renderer` 选项在运行时选择走哪条渲染路径——不需要单独的构建产物。

DLL 的生成位置（与仓库其余部分完全一致，详见 `docs/building.md`）：

| 路径 | 说明 |
| --- | --- |
| `build/release/Release/tkd2dcanvas01.dll` | `cmake --preset windows-release` 的默认产物 |
| `build/debug/Debug/tkd2dcanvas01.dll` | `cmake --preset windows-debug` |
| `build/default/Release/tkd2dcanvas01.dll` | `cmake --preset windows-default` |
| `python/tkd2dcanvas/tcl/tkd2dcanvas/tkd2dcanvas01.dll` | 构建后自动同步（`TKG_SYNC_PYTHON_PACKAGE=ON`，默认开）——**Python 实际加载的就是这个** |
| `build/wheel/Release/tkd2dcanvas01.dll` | `python tools/build_windows_wheel.py` 的中间产物 |

Python 包加载的是 `python/tkd2dcanvas/tcl/tkd2dcanvas/` 下那一份，
`cmake --build` 之后它会被自动覆盖，所以改完 C++ 代码只要重新
`cmake --build build/release --config Release` 就能在 Python 里生效，
不必重装 wheel。

### 8.3 与 Tk 侧实现的差异

* 文字同样交给 GDI（`ExtTextOutW`），由 GDI 自己灰度抗锯齿；
* 图片走 `AlphaBlend`（带 alpha 的 PNG 正常显示）；
* 虚线/stipple 仍然按 GDI 的设备像素长度绘制，不会随超采样放大；
* 每个画布控件有自己的渲染器和自己的离屏缓冲。

### 8.4 踩过的坑（写在这里以免重蹈覆辙）

1. **`AlphaBlend` 的 `AlphaFormat` 必须是 `AC_SRC_ALPHA`（0x01）**。
   写成 `255` 时 `AlphaBlend` 会以 `ERROR_INVALID_PARAMETER` 失败——而且是
   **静默**失败：形状一个都画不出来，只有肉眼对比才发现"边缘是锯齿的"。
2. **32 位 DIB 的像素是 B,G,R,A**。把颜色按 `0xAARRGGBB` 写进 DWORD 就等于
   把红蓝通道换了个个儿。
3. **世界变换是 DC 状态的一部分**。画旋转文字/图片时用了
   `SetWorldTransform`，画完必须 `RestoreDC` 还原——否则后面把后备缓冲
   `BitBlt` 到窗口的那一步也会被旋转，结果整个画布是空白的。
4. **`-width 0` 的描边不画**（Tk 和 Direct2D 都是这个行为）。GDI 的
   `CreatePen` 会把宽度 0 变成 1，于是凭空多出一圈边框。
5. **`AlphaBlend` 的**源**矩形不能超出源位图**。实测：目标矩形超出设备
   会被正常裁剪（`ok=1`），但源矩形越界会以 `ERROR_INVALID_PARAMETER`
   失败——图片要按源位图尺寸裁剪。
6. **路径不能在取整之前丢掉小数部分**。`AlignStrokePath` 会把描边移到像素
   网格上（差半个像素），这个偏移必须保留到超采样那一步：如果先把坐标取整，
   1 像素的边框就会摊到两个像素上、各占 50%，看起来又软又宽。
   所以抗锯齿引擎用的是 `AaPoint`（双精度坐标），不是 `POINT`。
7. **`PS_ENDCAP_ROUND` 不是对称的**。GDI 把圆头画成弦线折线，上下两半覆盖率
   不相同（3px 圆头对齐后：上半弧 63/223、下半弧 111/255），而且圆头的顶点
   正好压在像素边界上，看起来是平的。所以开放路径 + round 端帽（实线）时，
   描边先按 flat 端帽画直边，再用解析半圆写入掩膜（`DrawRoundCapEnd`），
   上下两半严格对称、覆盖率连续。虚线端帽仍然回退到 GDI 的 round cap。
8. **`SetROP2` 会让 GDI 在 32bpp DIB 上用背景色画图**。引擎里特意不设 ROP，
   保持默认的 `SRCCOPY`。

### 8.5 内存

抗锯齿引擎的离屏缓冲只增不减：画过一次大图形之后，缓冲会一直保持那么大。
每个画布控件都有自己的缓冲，所以这里做了限制——超过保留上限（4M 采样点 /
1M 像素）的缓冲在画完之后立刻释放。实测滚动时 GDI 句柄数与内存占用都是恒定的。

### 8.6 与脏矩形重绘的关系（"画面割裂"是怎么来的）

画布沿用 Direct2D 后端那套**脏矩形**重绘：只有损坏的区域被重画
（见 `docs/architecture.md`）。这意味着脏矩形的边界**可能正好切在一条抗锯齿
边缘的中间**。这是"画面割裂"最容易被怀疑的来源，实测下来并不是：

| 检查 | 方法 | 结果 |
| --- | --- | --- |
| 抗锯齿边缘能否被裁剪截断 | 引擎层：把裁剪边界压在椭圆边缘上，比对可见像素 | **0 / 26000 像素不同** |
| 局部重绘与整屏重绘是否一致 | 画布层：同一最终画面，一次整屏刷新、一次靠脏矩形刷新 | **0 / 120000 像素不同**（D2D 同样为 0） |

原因是引擎的工作方式：它把**整个形状**渲染到离屏缓冲，再整块合成到目标。
裁剪只决定"哪些像素被写"，不参与"像素长什么样"——覆盖率是在没有裁剪的
情况下算好的。所以脏矩形切在边缘中间不会截断边缘。

真正带来"割裂感"的是另外两件事，都是手动抗锯齿的固有代价：

**（一）覆盖率量化。** N 倍超采样意味着每个像素只有 N×N 个采样点，
覆盖率只能取 N×N+1 个离散值：

| 超采样倍数 | 采样点 | 覆盖率级数 | 相邻灰阶差 |
| --- | --- | --- | --- |
| 2 | 4 | 5 | 51/255 |
| 4（默认） | 16 | 17 | 16/255 |
| 8 | 64 | 65 | 4/255 |
| D2D `PER_PRIMITIVE` | 解析 | 256 | 1/255 |

边缘的灰度因此是"台阶状"的，而不是连续渐变。实测同一条缓斜线：
4 倍有 15 个灰阶，8 倍有 41 个。**这就是最直接的"画面割裂"来源**，
调 `-antialiasfactor 8` 能明显改善（代价见 8.7）。

**（二）相邻图元之间的接缝。** 每个图元各自独立做抗锯齿、再各自
source-over 合成。共享边上的两个半覆盖像素各叠一次，背景会漏进来：

| 画法 | 共享边像素（蓝底白背景） | 覆盖率 |
| --- | --- | --- |
| 一个矩形，右边在 x=32.5 | `FF8080FF` | 50% |
| 两个矩形共享 x=32.5 | `FF4040FF` | 25% |

背景漏了 25%，交界处就比两侧浅一点，看上去像一条缝。这条性质
**D2D 也有**（`PER_PRIMITIVE` 同样是逐图元混合），不是 GDI 独有的缺陷。

### 8.7 滚动：两个后端都是"移位"，不是重新渲染

滚动路径是两个后端**共用**的（`native/tk_glue/CanvasWidget.cpp` 的
`RepaintAfterScroll`），与渲染器无关：

1. `surface.ScrollContent()` → **后备缓冲内部 BitBlt**（像素移位，不重画）
2. `ScrollWindowEx()` → 窗口内容移位
3. 只让**新暴露的条带**失效（外加 1 像素的接缝带）
4. `UpdateWindow()` → 只重画那些条带

所以滚动时，**保留区的图元是纯移位，不会被重新渲染**。实测：

| 检查 | D2D | GDI |
| --- | --- | --- |
| 滚动后保留区 vs "原像素直接移位" | 逐字节相同（0 像素不同） | 逐字节相同（0 像素不同） |
| 滚动一帧耗时（200 个图元、600×400） | 1.45 ms | **0.10 ms** |
| 滚动耗时随场景图元数增长 | 不增长 | 不增长 |

滚动开销与场景里有多少图元**无关** —— 这正是"移位而非重绘"的判据
（如果是全量重绘，60 行和 240 行的耗时会差 4 倍）。

### 8.8 性能

GDI 后端每个图元都要"超采样 → 降采样 → AlphaBlend"，比 D2D 慢。
实测（200 个 24×24 的椭圆，600×400 画布）：

| 场景 | D2D | GDI（优化前） | GDI（优化后） |
| --- | --- | --- | --- |
| 滚动一帧 | 1.45 ms | 0.48 ms | **0.10 ms** |
| 整屏重绘 | 1.21 ms | 4.24 ms | **0.16 ms** |

> **一个重要的优化**：画布重绘时**背景矩形也走抗锯齿引擎**
> （`Win32Surface::Paint` 用 `fillPath` 填背景）。但背景是轴对齐、
> 整数坐标的矩形 —— 它**根本没有抗锯齿边缘**，却被要求走一遍
> N 倍超采样：600×400 的背景，引擎要 4.0 ms，普通 `FillRect` 只要 0.016 ms
> （250 倍）。
>
> 因此 `GdiRenderer::fillPath` 加了快路径：**轴对齐 + 整数坐标**的矩形
> 直接用 `FillRect`。这类矩形每个像素要么被完全覆盖、要么完全没被覆盖，
> 降采样后的结果和 `FillRect` 逐字节相同（`tests/gdi_aa/aa_rect_equiv.cpp`
> 守住这个等价关系）。实测优化前后画面**逐像素完全一致**，
> 整屏重绘从 4.24 ms 降到 0.16 ms。
>
> 半像素坐标的矩形、斜边、多边形等仍然走抗锯齿引擎 —— 它们确实需要。

### 8.9 已知限制

* **`-renderer` 只能在创建时指定。** 渲染器是在 `AttachToWindow()` 时创建的，
  而 `SetRendererKind()` 只改枚举、不重建渲染器。运行时
  `configure -renderer gdi` **不会**切换后端（实测：切换前后边缘灰阶数
  不变，仍是 D2D 的 18 而不是 GDI 的 13）。要换后端请新建控件。
* **文字仍由 GDI 自己栅格化**（`ExtTextOutW` + ClearType 质量），与 D2D 的
  DirectWrite 排版和栅格化不同。滚动场景下两个后端 8.6% 的像素差异里，
  约 20% 来自文字区（x 170–300），其余是图形区——这部分是覆盖率量化
  和逐图元接缝造成的。
* 虚线/stipple 不随超采样放大；超大图元回退到普通 GDI（无抗锯齿）。

### 8.10 验证

```powershell
# 抗锯齿引擎的单元测试（不需要窗口）
powershell -ExecutionPolicy Bypass -File tests\gdi_aa\build_aa_render_test.ps1 -Run

# 边缘特性探针（裁剪安全性 / 覆盖率量化 / 相邻图元接缝）
powershell -ExecutionPolicy Bypass -File tests\gdi_aa\build_aa_edge_probe.ps1 -Run

# 像素对齐矩形：抗锯齿引擎 vs FillRect 的等价性（快路径的正确性依据）
powershell -ExecutionPolicy Bypass -File tests\gdi_aa\build_aa_rect_equiv.ps1 -Run

# 两个后端逐像素对比（普通场景 / 滚动后的场景）
python tests\gdi_aa\compare_backends.py
python tests\gdi_aa\compare_scrolled.py

# 重绘耗时对比
python tests\gdi_aa\bench_repaint.py

# TinUI 演示（三个后端并排）
python tests\tinui\run_tinui.py --backend gdi --capture out.png
```

`tests/gdi_aa/aa_render_test.cpp` 会检查填充色是不是**请求的颜色**（红蓝不能
互换）、斜边是不是真的被平滑了、描边是不是用了画笔的颜色——上面第 1、2 条
坑就是它抓出来的。

`tests/gdi_aa/aa_edge_probe.cpp` 把 8.6 节的三条性质变成可回归的断言，
输出如下：

```text
1. clip safety: 0 of 26000 visible pixels differ
2. coverage quantization: 15 gray levels at scale 4, 41 at scale 8 (D2D: analytic, 256)
3. shared edge: one shape FF8080FF, two shapes FF4040FF (white = FFFFFFFF)
aa_edge_probe: all checks passed
```

`tests/gdi_aa/aa_rect_equiv.cpp` 守住 8.8 节快路径的正确性 ——
像素对齐的矩形，抗锯齿引擎和 `FillRect` 必须画出**完全相同的 RGB**
（只有 32 位 DIB 里那个没用的 alpha 字节不同）：

```text
rect ( 10, 10)-( 40, 30): 0 pixels differ
rect ( 11, 13)-( 47, 39): 0 pixels differ
rect (  0,  0)-(200,200): 0 pixels differ
rect ( 64, 64)-( 65, 65): 0 pixels differ
rect ( 10, 20)-(190, 21): 0 pixels differ
rect ( 37, 41)-( 38, 42): 0 pixels differ
aa_rect_equiv: all checks passed
```


