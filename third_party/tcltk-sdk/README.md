# tcltk-sdk（Tcl/Tk 开发文件，可选）

这里放置本项目构建所需的 Tcl/Tk **开发文件**（头文件 + stub 库），**不放入版本库**。
`FindTclTkDev.cmake` 会优先在此目录查找；找不到时再尝试 `TCL_ROOT` / `TK_ROOT`。

## 期望布局

```text
third_party/tcltk-sdk/
├── include/
│   ├── tcl.h
│   ├── tk.h
│   ├── tclDecls.h
│   ├── tkDecls.h
│   ├── tclPlatDecls.h     ← Windows 版平台声明头（tcl.h 会 include 它）
│   ├── tkPlatDecls.h
│   └── X11/               ← Tk 的 Windows 构建需要 X11 兼容头
│       ├── Xlib.h
│       ├── X.h
│       ├── Xutil.h
│       └── keysym.h ...
└── lib/
    ├── tclstub86.lib
    └── tkstub86.lib
```

> `tclPlatDecls.h` / `tkPlatDecls.h` 在 Tcl/Tk 源码树的 `generic/` 目录
> （Windows 版本；Unix 版在 `unix/`），拷贝 SDK 时容易漏掉，漏掉会报
> “无法打开包括文件 tclPlatDecls.h”。
> `X11/*.h` 在 `tk/xlib/X11/`，漏掉会报 “无法打开包括文件 X11/Xlib.h”。

> 一次构建只对应一个 Tcl/Tk major.minor。第一版建议对齐
> **Python 自带的 Tcl/Tk 8.6**（python.org 安装器与官方 Windows 构建均为 8.6）。
> 不要用 Tcl/Tk 9 的 SDK 去构建给 8.6 运行的扩展。

## 获取方式

任选其一：

1. **从源码构建（推荐，版本可控）**：见仓库根 README / docs/building.md 中的命令。
2. **使用系统已安装的 Tcl/Tk**：传 `-DTCL_ROOT=<前缀> -DTK_ROOT=<前缀>`，
   前缀下含 `include/` 与 `lib/`。

验证：

```powershell
python tools/check_tcltk_dev.py
# 或
python tools/find_tcltk.py --root third_party/tcltk-sdk
```

## 许可说明

本项目自身为 **LGPL-3.0-or-later**（见根目录 `LICENSE` / `COPYING`）。

Tcl/Tk 以 BSD 风格许可发布，与 LGPLv3 兼容。即使仅以 stub 方式链接、或在安装包中
捆绑 Tcl/Tk，也必须保留其版权声明与许可证文本。构建 wheel / 分发包时请在安装包中
附带 Tcl/Tk 的 `license.terms`（源码仓库根目录自带）。
