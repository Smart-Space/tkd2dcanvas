# Wheel 打包说明

## 产物

`tkd2dcanvas` 的原生产物是 Tcl package（不是普通 `.pyd`）：

```text
tkd2dcanvas-0.1.0-py3-none-win_amd64.whl
```

解压后：

```text
tkd2dcanvas/
├── __init__.py
├── _loader.py, canvas.py, compat.py, events.py, options.py, items.py, errors.py
├── py.typed
└── tcl/
    └── tkd2dcanvas/
        ├── pkgIndex.tcl
        └── tkd2dcanvas01.dll   ← 唯一原生二进制
```

## 构建命令

```powershell
python -m pip install build
python tools/build_windows_wheel.py
```

脚本会先跑 SDK 检查，再 cmake Release 构建，然后把 DLL 同步进
`python/tkd2dcanvas/tcl/tkd2dcanvas/`，最后在临时 staging 目录中
执行 `python -m build`，wheel 输出到 `dist/`。

## 安装与验证

```powershell
pip install dist/tkd2dcanvas-0.1.0-py3-none-win_amd64.whl
python -c "import tkd2dcanvas; print(tkd2dcanvas.__version__)"
```

## 注意事项

- **Tcl/Tk 版本必须匹配**：目标解释器是 Python 自带的 Tcl/Tk（Windows 上为 8.6）。
  用 8.6 构建、只给 8.6 分发；不要在 wheel 里混入 tcl86t.dll，靠系统/Python 提供的
  运行库加载。
- **DLL 架构**：只发布 x64（`tclstub86.lib` 的 x64 版本）。32 位 Python 不支持。
- **stub 链接**：扩展按 stub 链接，运行时由 `package require` 时的解释器提供
  实际符号——不要静态链接 Tcl/Tk。
- **许可**：本项目为 **LGPL-3.0-or-later**（`LICENSE` = LGPLv3，`COPYING` = GPLv3）。
  wheel 中捆绑的只有本项目的 DLL 与 Tcl 脚本，均属 LGPLv3。
  运行时使用用户的 Tcl/Tk，无需捆绑 Tcl/Tk 本体；若日后捆绑，必须在包内
  附带 Tcl/Tk 的 `license.terms`（BSD）并保留 LGPL/GPL 声明。
  分发 DLL 时应附上 LGPLv3 与 GPLv3 全文，并允许用户替换/重新链接该 DLL
  （LGPL §4(d)(1) 的共享库机制已满足：动态链接 Tcl stub，替换 DLL 即可）。
- **符号导出**：`Tkd2dcanvas_Init` / `Tkd2dcanvas_SafeInit` 需要
  `__declspec(dllexport)`（已在 `Tkd2dcanvasInit.cpp` 中）。
- **杀毒软件误报**：新编译的 DLL 可能被误报；保持代码签名（将来）并
  提供校验和有助于缓解。
- **验证清单**（发布前）：
  1. 干净虚拟机 + 官方 python.org Python 安装 wheel；
  2. `python -c "import tkinter; root=tkinter.Tk(); import tkd2dcanvas; tkd2dcanvas.load_into(root); root.destroy()"` 不报错；
  3. `wish` 中 `package require tkd2dcanvas` 成功（用 Python 自带的 `_tkinter` 亦可）。
