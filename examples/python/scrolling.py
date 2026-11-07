"""Scrolling demo: scrollregion larger than the viewport + scrollbars."""

import tkinter

import tkd2dcanvas


def main() -> None:
    root = tkinter.Tk()

    vbar = tkinter.Scrollbar(root, orient="vertical")
    hbar = tkinter.Scrollbar(root, orient="horizontal")
    canvas = tkd2dcanvas.Canvas(
        root,
        width=480,
        height=320,
        background="white",
        scrollregion=(0, 0, 1600, 1200),
        xscrollcommand=hbar.set,
        yscrollcommand=vbar.set,
    )

    vbar.config(command=canvas.yview)
    hbar.config(command=canvas.xview)

    vbar.pack(side="right", fill="y")
    hbar.pack(side="bottom", fill="x")
    canvas.pack(side="left", fill="both", expand=True)

    for i in range(12):
        canvas.create_rectangle(
            40 + i * 120, 40 + i * 90,
            160 + i * 120, 120 + i * 90,
            fill="#4f81bd" if i % 2 == 0 else "#c0504d",
        )

    root.mainloop()


if __name__ == "__main__":
    main()
