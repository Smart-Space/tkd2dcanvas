# Python API（草案）

> 当前状态：所有方法均已定义签名，原生实现按 milestones 落地
> （见 [roadmap.md](roadmap.md)）。未实现的命令会返回清晰的
> `CommandError`（"not implemented yet (milestone Mxx)"）。

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

## 查找与 tag

```python
canvas.addtag_withtag("hot", r)          # 按 tagOrId 打 tag
canvas.addtag_all("hot")
canvas.addtag_overlapping("hot", 0, 0, 100, 100)
canvas.find_withtag("hot")               # -> [ids]
canvas.find_all()
canvas.find_closest(50, 50)              # -> [id]
canvas.find_overlapping(0, 0, 100, 100)
canvas.find_enclosed(0, 0, 100, 100)
canvas.dtag(r, "hot")
canvas.gettags(r)                        # -> ["hot", ...]
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

## 异常

- `tkd2dcanvas.LoadError`：Tcl package 加载失败（DLL 缺失/版本不匹配）。
- `tkd2dcanvas.CommandError`：Tcl 命令执行失败或未实现。
