"""
Side-by-side viewer: tkinter.Canvas vs tkd2dcanvas.Canvas.
"""

import sys
from pathlib import Path

_ROOT = Path(__file__).resolve().parents[1]
_PY = str(_ROOT / "python")
if _PY not in sys.path:
    sys.path.insert(0, _PY)

import tkinter

import tkd2dcanvas

VIEW_W, VIEW_H = 620, 460
SCROLL_REGION = (0, 0, 1600, 1200)
GRID_STEP = 100

DPI_AWARE = False


def apply_dpi_awareness():
    if not DPI_AWARE:
        return
    import ctypes
    user32 = ctypes.windll.user32
    try:
        context = ctypes.c_void_p(-4)
        if user32.SetProcessDpiAwarenessContext(context):
            return
    except (AttributeError, OSError):
        pass


def draw_scene(canvas):
    """The single piece of canvas code that both panes run.

    Items the native side does not support yet are recorded as "skipped"
    instead of aborting the redraw, so the panes stay comparable.
    """
    x1, y1, x2, y2 = SCROLL_REGION
    total = 0
    skipped = {}

    def item(create, *args, **kw):
        nonlocal total
        total += 1
        try:
            create(*args, **kw)
        except Exception as exc:
            name = create.__name__
            count, _ = skipped.get(name, (0, None))
            skipped[name] = (count + 1, exc)

    # Grid + rulers (make scrolling offsets visible).
    for x in range(0, x2 + 1, GRID_STEP):
        color = "#c0c0d8" if x % 500 == 0 else "#ececf2"
        item(canvas.create_line, x, y1, x, y2, fill=color)
        if x % 200 == 0:
            item(canvas.create_text, x + 5, y1 + 4, anchor="nw",
                 text=str(x), fill="#909098", font=("Segoe UI", 8))
    for y in range(0, y2 + 1, GRID_STEP):
        color = "#c0c0d8" if y % 500 == 0 else "#ececf2"
        item(canvas.create_line, x1, y, x2, y, fill=color)
        if y % 200 == 0:
            item(canvas.create_text, x1 + 4, y + 5, anchor="nw",
                 text=str(y), fill="#909098", font=("Segoe UI", 8))

    logo = tkinter.PhotoImage(file=str(_ROOT / "docs" / "logo.png")).subsample(4, 4)
    if not hasattr(canvas, "_compare_photos"):
        canvas._compare_photos = []
    canvas._compare_photos.append(logo)
    item(canvas.create_image, 760, 60, image=logo, anchor="nw")

    # Filled rectangle / oval, shared with examples/python/basic_shapes.py.
    item(canvas.create_rectangle, 20, 20, 200, 120,
         fill="tomato", outline="navy", width=2)
    item(canvas.create_oval, 220, 20, 400, 140,
         fill="lightblue", outline="black")

    # Wide line with configurable cap/join styles.
    item(canvas.create_line, 20, 180, 400, 180, fill="black", width=6,
         capstyle="round", joinstyle="round")
    item(canvas.create_line, 20, 220, 200, 260, 380, 240, 380, 320,
         fill="steelblue", width=4, joinstyle="bevel")

    # Dashed lines (two numeric dash patterns).
    item(canvas.create_line, 20, 360, 400, 360, fill="darkgreen",
         width=2, dash=(6, 4))
    item(canvas.create_line, 20, 400, 400, 400, fill="firebrick",
         width=2, dash=(2, 4))

    # Polygon and arc (pie slice) with an outline.
    item(canvas.create_polygon, 460, 40, 600, 120, 460, 200,
         fill="gold", outline="darkgreen", width=9)
    item(canvas.create_arc, 20, 440, 220, 640, start=30, extent=90,
         style="pieslice", fill="orchid", outline="purple")

    # Text with anchors / fonts.
    item(canvas.create_text, 440, 260, anchor="nw", fill="black",
         text="anchor nw / Segoe UI 14", font=("Segoe UI", 14))
    item(canvas.create_text, 640, 200, anchor="ne", fill="#404040",
         text="anchor ne\n两行中文，更长文本自动换行", width=120,
         font=("Microsoft YaHei", 12), justify="right")
    item(canvas.create_text, 500, 340, anchor="center", fill="crimson",
         text="anchor center",
         font=("Consolas", 16, "italic"))
    item(canvas.create_text, 500, 420, anchor="w", fill="seagreen",
         text="angle 45", font=("Segoe UI", 14), angle=45)

    # A marker at the far corner to prove the pane scrolled all the way.
    item(canvas.create_rectangle, 1570, 1150, 1580, 1160,
         fill="black", outline="black")

    return total, skipped


def make_canvas(cls, master):
    """Shared constructor kwargs so both panes are configured identically."""
    return cls(
        master,
        width=VIEW_W,
        height=VIEW_H,
        background="white",
        highlightthickness=0,
        scrollregion=SCROLL_REGION,
    )


class Pane:

    other = None  # the pane this one mirrors to
    _syncing = False  # re-entry guard between the two panes

    def __init__(self, parent, title, canvas_cls):
        self.frame = tkinter.LabelFrame(parent, text=title)
        self.canvas = make_canvas(canvas_cls, self.frame)

        self.hbar = tkinter.Scrollbar(
            self.frame, orient="horizontal", command=self.canvas.xview)
        self.vbar = tkinter.Scrollbar(
            self.frame, orient="vertical", command=self.canvas.yview)

        self.canvas.grid(row=0, column=0, sticky="nsew")
        self.vbar.grid(row=0, column=1, sticky="ns")
        self.hbar.grid(row=1, column=0, sticky="ew")
        self.frame.rowconfigure(0, weight=1)
        self.frame.columnconfigure(0, weight=1)

        self.canvas.configure(
            xscrollcommand=self._on_x,
            yscrollcommand=self._on_y,
        )

    def _on_x(self, first, last):
        self.hbar.set(first, last)
        self._mirror("xview", first)

    def _on_y(self, first, last):
        self.vbar.set(first, last)
        self._mirror("yview", first)

    def _mirror(self, view, first):
        other = self.other
        if other is None or Pane._syncing:
            return
        Pane._syncing = True
        try:
            getattr(other.canvas, view)("moveto", first)
        finally:
            Pane._syncing = False


def main():
    apply_dpi_awareness()
    root = tkinter.Tk()
    root.title("tkinter.Canvas vs tkd2dcanvas.Canvas")

    left = Pane(root, "tkinter.Canvas", tkinter.Canvas)
    right = Pane(root, "tkd2dcanvas.Canvas", tkd2dcanvas.Canvas)
    left.other = right
    right.other = left

    n1, skip1 = draw_scene(left.canvas)
    n2, skip2 = draw_scene(right.canvas)

    def summary(n, skip):
        drawn = n - sum(count for count, _ in skip.values())
        text = "{}/{}".format(drawn, n)
        if skip:
            parts = ["{} ×{} ({})".format(name, count, err)
                     for name, (count, err) in skip.items()]
            text += "  |  skipped: " + "; ".join(parts)
        return text

    status = "tkinter: {}  tkd2d: {}".format(summary(n1, skip1),
                                             summary(n2, skip2))

    tkinter.Label(root, text=status).pack(side="bottom", fill="x")
    left.frame.pack(side="left", fill="both", expand=True, padx=(8, 4), pady=8)
    right.frame.pack(side="left", fill="both", expand=True, padx=(4, 8), pady=8)

    root.mainloop()


if __name__ == "__main__":
    main()
