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
