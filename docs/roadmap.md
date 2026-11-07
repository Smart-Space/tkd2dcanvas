# 路线图（里程碑）

> 完整设计与风险分析见 [规划.md](规划.md) §12/§13/§15。此处跟踪落地进度。

## 阶段 0：环境验证（M0）✅ 已完成（2026-11-07）

- [x] Tcl/Tk 8.6 dev SDK 就位（`python tools/check_tcltk_dev.py` 通过 → 8.6.18）
- [x] `cmake --preset windows-release` 配置成功，产出 `tkd2dcanvas01.dll`
- [x] `package require tkd2dcanvas` 成功（返回 0.1，命令已注册）
- [x] Tcl 测试通过（Total 6 / Passed 6 / Failed 0）、Python 测试 6 passed

## 阶段 1：空 widget（M1 + M2）

- [ ] `tkd2dcanvas .c -width 400 -height 300` 创建 widget
- [ ] Tk window 显示、`configure`/`cget`、几何请求、销毁路径
- [ ] 事件桩（Expose/Configure/Focus/Destroy）

## 阶段 2：Direct2D 背景（M3）

- [ ] `-background lightgray` 显示灰色背景
- [ ] 双缓冲消闪烁、`WM_PAINT` 正确、resize 不拉伸

## 阶段 3：基本 item（M4）

- [ ] rectangle / oval / line（含 `-fill`、`-outline`、`-width`、`-dash`）
- [ ] item id、`create` 返回、重绘脏区

## 阶段 4：基础命令（M5）

- [ ] `coords` / `move` / `delete` / `itemconfigure` / `itemcget` / `bbox`
- [ ] widget 级 `configure` / `cget`

## 阶段 5：tag / find / bind（M6）

- [ ] `addtag` / `dtag` / `find` (withtag/all/closest) / `gettags`
- [ ] `tag_bind`：C++ core 命中测试 + Tcl 脚本派发

## 阶段 6：滚动（M7）

- [ ] `xview` / `yview`（moveto/scroll/units/pages）
- [ ] `-xscrollcommand` / `-yscrollcommand` / `-scrollregion` / `-*scrollincrement`

## 阶段 7：Python 包装（M8）

- [ ] wheel 可安装、`load_into` 健壮、常见选项序列化
- [ ] `tkinter.Canvas` 兼容层（见 tk_canvas_compat_matrix.md）

## 阶段 8：图像与文字增强（M9）

- [ ] `create_text` 完整（font/anchor/justify/width/wrap）
- [ ] `create_image`（Tk photo → D2D bitmap）
- [ ] font 解析（`font actual`）、`create_bitmap`

## 阶段 9：Window item（M10）

- [ ] `create_window`：真实 Tk 子 HWND 嵌入

## 阶段 10：打包与发布（M11）

- [ ] wheel（x64）、安装验证、文档、签名/校验
