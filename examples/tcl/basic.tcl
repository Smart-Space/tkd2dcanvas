# Basic shapes demo (see examples/python/basic_shapes.py)
# The widget itself works since M2; the item commands need M4.
package require Tk
package require tkd2dcanvas

frame .root
pack .root -fill both -expand 1

tkd2dcanvas .c -width 640 -height 480 -background white
pack .c -in .root -fill both -expand 1

# M4 will add these:
# .c create rectangle 20 20 200 120 -fill tomato -outline navy -width 2
# .c create oval 220 20 400 140 -fill lightblue -outline black
# .c create line 20 180 400 180 -fill black -width 2
