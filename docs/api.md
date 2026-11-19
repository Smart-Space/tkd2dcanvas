# Python API（草案）

> 当前状态：所有方法均已定义签名，原生实现按 milestones 落地
> （见 [roadmap.md](roadmap.md)）。未实现的命令要么不存在，要么抛出
> `CommandError` / `NotImplementedError`（如 `focus`），绝不会调用不存在的
> Tcl 子命令。

## 加载

```python
import tkd2dcanvas
tkd2dcanvas.load_into(root)   # 幂等
```

## 创建与打包

```python
canvas = tkd2dcanvas.Canvas(root, width=800, height=600, background="white")
canvas.pack(fill="both", expand=True)     # 或 grid()/place()
```

构造参数同 Tk widget options（`-width` → `width=...`）。

## Items

```python
r = canvas.create_rectangle(20, 20, 200, 120, fill="tomato", outline="navy")
o = canvas.create_oval(220, 40, 380, 160, fill="lightblue")
l = canvas.create_line(0, 0, 300, 300, fill="black", width=2)
t = canvas.create_text(40, 40, text="hello", anchor="nw", font=("Helvetica", 24, "bold"))
```

返回 Tk item id（`int`）。

## 坐标与移动

```python
x1, y1, x2, y2 = canvas.coords(r)
canvas.coords(r, 30, 30, 210, 130)
canvas.move(r, 10, 5)
canvas.moveto(r, 100, 100)
```

## 选项

```python
canvas.itemconfigure(r, fill="red")
canvas.itemcget(r, "fill")
canvas.configure(scrollregion=(0, 0, 2000, 2000))
canvas.cget("width")
```

### 渲染后端

| 选项 | 默认值 | 说明 |
| --- | --- | --- |
| `renderer` | `"d2d"` | `"d2d"`（Direct2D/DirectWrite）或 `"gdi"`（纯 GDI + 手动抗锯齿） |
| `antialiasfactor` | `4` | `gdi` 后端的超采样倍数 N（2…8）。越大边缘越平滑、越慢 |

```python
canvas = tkd2dcanvas.Canvas(root, width=400, height=300, renderer="gdi")
```

两个后端在**同一个 DLL** 里，构建产物位置见
[building.md](building.md)；`-renderer` **只能在创建时指定**，
运行时 `configure(renderer=...)` 不会切换后端。

`gdi` 后端的技术细节、限制与实测数据见
[gdi_antialias.md](gdi_antialias.md)。

## 查找与 tag

```python
canvas.addtag_withtag("hot", r)          # 按 tagOrId 打 tag
canvas.addtag_all("hot")
canvas.addtag_above("hot", r)            # r 之上的 item
canvas.addtag_below("hot", r)            # r 之下的 item
canvas.addtag_closest("hot", 50, 50)     # 最近的 item（支持 halo/start）
canvas.addtag_overlapping("hot", 0, 0, 100, 100)
canvas.find_withtag("hot")               # -> (ids, ...) tuple
canvas.find_all()
canvas.find_closest(50, 50)              # -> (id, ...)
canvas.find_overlapping(0, 0, 100, 100)
canvas.find_enclosed(0, 0, 100, 100)
canvas.dtag(r, "hot")
canvas.gettags(r)                        # -> ("hot", ...)
canvas.tag_raise(r)
canvas.tag_lower(r)
```

## 绑定

```python
from tkd2dcanvas import events

def on_click(event):
    print(event.x, event.y, event.type)

funcid = canvas.tag_bind("hot", events.BUTTON_PRESS_1, on_click)
canvas.bind(events.BUTTON_PRESS_1, on_click)
```

## 滚动

```python
canvas.configure(scrollregion=(0, 0, 2000, 2000),
                 xscrollcommand=hbar.set, yscrollcommand=vbar.set)
canvas.xview("moveto", 0.5)
first, last = canvas.yview()
```

## 缩放与坐标映射

```python
canvas.scale("all", 0, 0, 2, 2) # 以原点按 (x,y)=(2,3) 缩放
canvas.scale(item, 100, 100, -1, -1) # 关于 (100,100) 镜像
canvas.canvasx(53) # window 坐标 -> canvas 坐标
canvas.canvasy(53, gridspacing=10) # 对齐到 10 的倍数
t = canvas.type("all") # 首个匹配 item 的类型；无匹配返回 None
```

## 异常

- `tkd2dcanvas.LoadError`：Tcl package 加载失败（DLL 缺失/版本不匹配）。
- `tkd2dcanvas.CommandError`：Tcl 命令执行失败或未实现。

## 兼容面（相对 tkinter.Canvas）

- `tkd2dcanvas.Canvas` 是 `tkinter.Canvas` 的子类——`winfo_*`、
  `pack`/`grid`/`place`、`nametowidget` 等行为与普通 Tk 控件一致。
- `find*` / `gettags` 与 tkinter 一致返回 **tuple**。
- 无参数的 `configure()` / `itemconfigure(item)` 返回 **dict**
  （`{选项名: 当前值}`），而不是 tkinter 的 `{name: (name, ..., 当前值)}`，
  键不带 `-` 前缀。
- 明确未实现（调用会抛 `NotImplementedError`）：
  `postscript`、`select_*`、`dchars`/`icursor`/`index`/`insert` 等文字 item 命令。
- 完整矩阵见 [tk_canvas_compat_matrix.md](tk_canvas_compat_matrix.md)。
