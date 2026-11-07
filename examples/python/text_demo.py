"""Text demo: create_text with fonts and anchors."""

import tkinter

import tkd2dcanvas


def main() -> None:
    root = tkinter.Tk()
    canvas = tkd2dcanvas.Canvas(root, width=640, height=400, background="white")
    canvas.pack(fill="both", expand=True)

    canvas.create_text(40, 40, text="tkd2dcanvas", anchor="nw",
                       font=("Helvetica", 28, "bold"), fill="navy")
    canvas.create_text(40, 100, text="DirectWrite rendered text (Tk font resolution)",
                       anchor="nw", font=("TkDefaultFont", 12), fill="gray25")
    canvas.create_text(600, 380, text="anchor=se\nmulti-line\nbottom-right",
                       anchor="se", fill="black")

    root.mainloop()


if __name__ == "__main__":
    main()
