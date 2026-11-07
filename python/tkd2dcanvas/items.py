"""Canvas item type constants mirroring Tk's create subcommands."""

RECTANGLE = "rectangle"
OVAL = "oval"
LINE = "line"
POLYGON = "polygon"
ARC = "arc"
TEXT = "text"
IMAGE = "image"
BITMAP = "bitmap"
WINDOW = "window"

# Item types supported by the native implementation.
SUPPORTED = [RECTANGLE, OVAL, LINE, POLYGON, ARC]
# Planned, not yet usable in 0.x: TEXT, IMAGE, BITMAP, WINDOW

ALL_TYPES = [RECTANGLE, OVAL, LINE, POLYGON, ARC, TEXT, IMAGE, BITMAP, WINDOW]
