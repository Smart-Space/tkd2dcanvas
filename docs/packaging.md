# 打包（wheel）说明

> 详细的 wheel 结构与发布清单见 [`packaging/wheel_notes.md`](../packaging/wheel_notes.md)。

## 目标

最终用户执行：

```powershell
pip install tkd2dcanvas
```

即可在**已有的 Python + Tcl/Tk 环境**（python.org 安装器，自带 Tk 8.6）中
直接使用：

```python
import tkd2dcanvas
tkd2dcanvas.load_into(root)
```

## 原则

1. 原生产物是 **Tcl package DLL**（`tkd2dcanvas01.dll`），不是 `.pyd`。
2. Python 包只负责：定位 Tcl package → `lappend auto_path` →
   `package require` → 提供 Pythonic 封装。
3. **不捆绑** Tcl/Tk 运行时（用用户自己的）；捆绑时需附带 BSD 许可声明。
4. Windows x64 only（当前）。

## 构建流程

```powershell
python -m pip install build
python tools/build_windows_wheel.py
```

步骤：SDK 校验 → CMake Release 构建 → DLL + pkgIndex.tcl 同步进
`python/tkd2dcanvas/tcl/tkd2dcanvas/` → staging（复制
`python/` + `packaging/pyproject.toml` + README/LICENSE）→
`python -m build --wheel` → `dist/*.whl`。

## 验证矩阵（发布前）

| 项 | 方法 |
| --- | --- |
| wheel 安装 | 干净 venv：`pip install dist/...whl` |
| import | `python -c "import tkd2dcanvas; print(tkd2dcanvas.__version__)"` |
| Tcl 加载 | `python -c "import tkinter; r=tkinter.Tk(); import tkd2dcanvas; tkd2dcanvas.load_into(r)"` |
| 许可证 | wheel 含 `LICENSE`（LGPLv3）与 `COPYING`（GPLv3）；若捆绑 Tcl/Tk，含其 `license.terms` |
