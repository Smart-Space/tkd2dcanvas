# 构建说明

## 前置条件

1. **Windows x64**（target 是 Direct2D/DirectWrite；其他平台不可用）。
2. **Visual Studio（MSVC）**：python.org 的 Python / `_tkinter` 是 MSVC 构建的，
   扩展用 MSVC 最省心。安装时勾选 "Desktop development with C++"。
3. **CMake ≥ 3.26**（仓库自带 `CMakePresets.json`）。
4. **Tcl/Tk 8.6 开发文件**（本仓库不捆绑）：见 `third_party/tcltk-sdk/README.md`。
   - 推荐版本：与运行 Python 自带的 Tcl/Tk 一致（Windows 上为 8.6）。
   - 放置位置：`third_party/tcltk-sdk/{include,lib}`；或用 `-DTCL_ROOT=... -DTK_ROOT=...`
     指向任意前缀（含 `include/tcl.h` 与 `lib/tclstub86.lib`）。

## 一、准备 Tcl/Tk SDK

### 1) 获取源码（以 8.6 最新补丁为例）

```powershell
git clone https://github.com/tcltk/tcl.git third_party/tcltk-src/tcl
git clone https://github.com/tcltk/tk.git  third_party/tcltk-src/tk

# 查看可选的 8.6 补丁 tag：
git -C third_party/tcltk-src/tcl tag -l "core-8-6-*"
git -C third_party/tcltk-src/tk  tag -l "core-8-6-*"

# 切到两个仓库中相同的最新 8.6 补丁 tag（替换为实际看到的名字）：
git -C third_party/tcltk-src/tcl checkout core-8-6-16
git -C third_party/tcltk-src/tk  checkout core-8-6-16
```

### 2) 用 MSVC 构建

打开 **"x64 Native Tools Command Prompt for VS"**（或 VS Installer 中的
Build Tools 命令提示），然后：

```bat
cd /d E:\Github\tkd2dcanvas\third_party\tcltk-src\tcl\win
nmake -f makefile.vc

cd /d E:\Github\tkd2dcanvas\third_party\tcltk-src\tk\win
nmake -f makefile.vc
```

产物通常在各自的 `Release_x64\` 目录（若不确定，`dir /s /b tclstub86.lib`）。

### 3) 拷贝开发文件到 SDK 目录

在仓库根目录执行（PowerShell）：

```powershell
$root  = "E:\Github\tkd2dcanvas"
$sdk   = "$root\third_party\tcltk-sdk"
New-Item -ItemType Directory -Force "$sdk\include", "$sdk\lib" | Out-Null

Copy-Item "$root\third_party\tcltk-src\tcl\generic\tcl.h"          "$sdk\include\"
Copy-Item "$root\third_party\tcltk-src\tcl\generic\tclDecls.h"      "$sdk\include\"
Copy-Item "$root\third_party\tcltk-src\tcl\generic\tclPlatDecls.h"  "$sdk\include\"
Copy-Item "$root\third_party\tcltk-src\tk\generic\tk.h"           "$sdk\include\"
Copy-Item "$root\third_party\tcltk-src\tk\generic\tkDecls.h"      "$sdk\include\"
Copy-Item "$root\third_party\tcltk-src\tk\generic\tkPlatDecls.h"  "$sdk\include\"

# Tk 的 Windows 构建还要 X11 兼容头（tk.h 会 include <X11/Xlib.h>）
New-Item -ItemType Directory -Force "$sdk\include\X11" | Out-Null
Copy-Item "$root\third_party\tcltk-src\tk\xlib\X11\*.h" "$sdk\include\X11\"

# 按实际构建输出目录调整下面两条：
Copy-Item "$root\third_party\tcltk-src\tcl\win\Release_x64\tclstub86.lib" "$sdk\lib\"
Copy-Item "$root\third_party\tcltk-src\tk\win\Release_x64\tkstub86.lib"   "$sdk\lib\"
```

> 漏掉 `tclPlatDecls.h` / `tkPlatDecls.h` → “无法打开包括文件 tclPlatDecls.h”；
> 漏掉 `X11/*.h` → “无法打开包括文件 X11/Xlib.h”。

验证：

```powershell
python tools\check_tcltk_dev.py
```

输出示例（已验证的机器）：

```text
Detected Tcl version: 8.6 (patch: 8.6.18)
OK: SDK looks usable.
```

## 二、配置与编译

```powershell
cmake --preset windows-release
cmake --build build/release --config Release
```

产物：`build/release/Release/tkd2dcanvas01.dll`。
默认 `TKG_SYNC_PYTHON_PACKAGE=ON`，构建后自动同步到
`python/tkd2dcanvas/tcl/tkd2dcanvas/`（含 pkgIndex.tcl）。

各个预设的产物位置：

| 预设 | DLL 产物 |
| --- | --- |
| `windows-release` | `build/release/Release/tkd2dcanvas01.dll` |
| `windows-debug` | `build/debug/Debug/tkd2dcanvas01.dll` |
| `windows-default` | `build/default/Release/tkd2dcanvas01.dll` |
| `tools/build_windows_wheel.py` | `build/wheel/Release/tkd2dcanvas01.dll`（再打进 `dist/*.whl`） |

**Python 实际加载的是 `python/tkd2dcanvas/tcl/tkd2dcanvas/tkd2dcanvas01.dll`**
——构建后由 `POST_BUILD` 步骤自动覆盖。改完 C++ 代码只要重新
`cmake --build build/release --config Release` 就能在 Python 里生效，
不必重装 wheel。

> 注：在 Tcl/Tk SDK 就位之前，`find_package(TclTkDev REQUIRED)` 会直接以清晰
> 的报错失败，这是刻意的（M0 门槛）。不依赖 Tcl/Tk 的 core 部分可以单独
> 构建：`cmake -S . -B build/bench -DTKG_CORE_ONLY=ON -DTKG_BUILD_BENCH=ON`。

## 三、冒烟验证

```powershell
# Tcl 层（无独立 wish/tclsh 时自动回退到 Python 自带的 Tk）
python tools/run_tcl_tests.py

# Python tkinter 层
python -c "import tkinter; r=tkinter.Tk(); import tkd2dcanvas; tkd2dcanvas.load_into(r); r.destroy()"

# 单元测试
ctest --test-dir build/release -C Release
python -m pytest tests/python -q
```

## 四、常见问题

- **找不到 stub 库**：确认路径含 `include/tcl.h` + `lib/tclstub86.lib`；
  或显式 `-DTCL_ROOT=C:\path\to\prefix -DTK_ROOT=...`。
- **版本不匹配**（`package require` 报 bad magic / 无法加载）：SDK 与运行
  Python 的 Tcl/Tk 不是同一 major.minor。统一用 8.6。
- **cl.exe 不在 PATH**：用 VS 开发人员命令提示符，或直接 cmake 预设（自动定位 VS）。
- **DLL 被杀软拦截**：自行构建或对产物签名。

## 五、清理

```powershell
Remove-Item -Recurse -Force build
```
