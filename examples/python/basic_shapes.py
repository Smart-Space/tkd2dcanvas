"""Basic shapes demo (rectangle / oval / line)."""

import tkinter

import tkd2dcanvas


def main() -> None:
    root = tkinter.Tk()
    canvas = tkd2dcanvas.Canvas(root, width=640, height=480, background="white")
    canvas.pack(fill="both", expand=True)

    canvas.create_rectangle(20, 20, 200, 120, fill="tomato", outline="navy", width=2)
    canvas.create_oval(220, 20, 400, 140, fill="lightblue", outline="black")
    canvas.create_line(20, 180, 400, 180, fill="black", width=2)

    root.mainloop()


if __name__ == "__main__":
    main()
