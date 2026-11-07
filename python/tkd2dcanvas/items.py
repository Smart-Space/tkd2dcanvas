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

# Item types supported by the native implementation once M4 lands.
SUPPORTED = [RECTANGLE, OVAL, LINE, TEXT]
# Planned, not yet usable in 0.x: POLYGON, ARC, IMAGE, BITMAP, WINDOW

ALL_TYPES = [RECTANGLE, OVAL, LINE, POLYGON, ARC, TEXT, IMAGE, BITMAP, WINDOW]
