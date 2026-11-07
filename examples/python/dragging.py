"""Dragging demo: move items with the mouse via tag_bind (M6)."""

import tkinter

import tkd2dcanvas
from tkd2dcanvas import events


def main() -> None:
    root = tkinter.Tk()
    canvas = tkd2dcanvas.Canvas(root, width=640, height=480, background="white")
    canvas.pack(fill="both", expand=True)

    rect = canvas.create_rectangle(40, 40, 160, 120, fill="tomato", tags=("draggable",))

    state = {"item": None, "start": (0.0, 0.0)}

    def on_press(event):
        state["item"] = rect
        state["start"] = (event.x, event.y)

    def on_motion(event):
        item = state["item"]
        if item is None:
            return
        sx, sy = state["start"]
        canvas.move(item, event.x - sx, event.y - sy)
        state["start"] = (event.x, event.y)

    def on_release(_event):
        state["item"] = None

    canvas.tag_bind("draggable", events.BUTTON_PRESS_1, on_press)
    canvas.tag_bind("draggable", events.MOTION, on_motion)
    canvas.tag_bind("draggable", events.BUTTON_RELEASE_1, on_release)

    root.mainloop()


if __name__ == "__main__":
    main()
