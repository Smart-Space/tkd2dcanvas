"""Image demo: Tk photo images drawn on the Direct2D canvas (M9)."""

import tkinter

import tkd2dcanvas


def main() -> None:
    root = tkinter.Tk()
    canvas = tkd2dcanvas.Canvas(root, width=640, height=480, background="white")
    canvas.pack(fill="both", expand=True)

    photo = tkinter.PhotoImage(master=root, name="demo_checker", width=64, height=64)
    for y in range(64):
        for x in range(64):
            color = "#222222" if (x // 8 + y // 8) % 2 == 0 else "#eeeeee"
            photo.put(color, to=(x, y))

    canvas.create_image(40, 40, image=photo, anchor="nw")

    root.mainloop()


if __name__ == "__main__":
    main()
