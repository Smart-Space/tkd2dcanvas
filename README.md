# tkd2dcanvas

在 Windows 上，用 Direct2D/DirectWrite 实现的 Canvas（面向 Tcl/Tk 与 Python tkinter）。

> 渲染后端为 **Direct2D / DirectWrite**（`d2d1` / `dwrite`，Win7 SP1+ 自带）。
> 运行时依赖 Windows 7 SP1+ 自带的 `d2d1.dll` / `dwrite.dll`，无需捆绑。
> 分支 `feature/gdi-antialias` 上另有一个**纯 GDI 后端**（`-renderer gdi`，
> 抗锯齿由本仓库自己的超采样引擎算），见
> [docs/gdi_antialias.md](docs/gdi_antialias.md)。

> 当前状态：代码骨架期（0.1.0）。**M0–M3 已完成**：Tcl package 可加载、
> `tkd2dcanvas .c -width 400 -height 300` 能创建真实 Tk widget 并用 Direct2D
> 绘制背景；item 命令（create/coords/move…）尚未实现。详见
> [docs/roadmap.md](docs/roadmap.md)。

## Python 侧最终目标

```python
from tkd2dcanvas import Canvas

canvas = Canvas(root, width=800, height=600)
canvas.pack(fill="both", expand=True)
canvas.create_rectangle(20, 20, 200, 120, fill="tomato", outline="navy")
```

底层实际发生的是：

```text
root.tk.call("tkd2dcanvas", pathName, ...)
root.tk.call(pathName, "create", "rectangle", ...)
```

## 目录结构

| 目录 | 作用 | 依赖 |
| --- | --- | --- |
| `native/core` | Canvas 场景图、几何、tag、命中测试 | 无外部依赖 |
| `native/items` | 各类 item（Rect/Oval/Line/...） | `core` |
| `native/render` | Renderer 抽象与 Direct2D/DirectWrite 实现 | core、d2d1/dwrite |
| `native/platform_win` | Win32 窗口、WM_PAINT、DPI、双缓冲 | Win32 |
| `native/tk_glue` | Tcl/Tk widget 胶水层 | Tcl/Tk |
| `tcl/` | Tcl package 安装文件（pkgIndex.tcl） | Tcl |
| `python/tkd2dcanvas` | Python API、loader、兼容层 | Python/tkinter |
| `tests/` | C++ / Tcl / Python 测试 | — |
| `tools/` | 环境检查、打包辅助脚本 | Python |
| `third_party/tcltk-sdk` | 可选的 Tcl/Tk 开发文件 | 见 `third_party/tcltk-sdk/README.md` |

构建与安装见 [docs/building.md](docs/building.md)，架构说明见 [docs/architecture.md](docs/architecture.md)，渲染性能基准见 [docs/performance.md](docs/performance.md)。

## 许可

本项目以 **LGPL-3.0-or-later** 发布：

- [LICENSE](LICENSE) —— GNU LGPL v3 全文
- [COPYING](COPYING) —— GNU GPL v3 全文（LGPLv3 引用其条款）

动态链接到 Tcl/Tk stub，运行时不静态链接任何第三方库。
构建产物中若捆绑 Tcl/Tk，Tcl/Tk 依其自身 BSD 许可，需保留其许可证声明
（见 `third_party/tcltk-sdk/README.md`）。
