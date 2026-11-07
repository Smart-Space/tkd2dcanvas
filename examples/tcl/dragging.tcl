# Dragging demo (see examples/python/dragging.py)
package require Tk
package require tkd2dcanvas

tkd2dcanvas .c -width 640 -height 480 -background white
pack .c -fill both -expand 1

# M4: set item [.c create rectangle 40 40 160 120 -fill tomato -tags draggable]
# M6: the bind commands below need the tag/bind implementation.
