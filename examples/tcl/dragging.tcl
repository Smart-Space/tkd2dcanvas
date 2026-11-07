# Dragging demo (see examples/python/dragging.py)
#
# M6: the tag bindings are dispatched by the canvas: the item under the
# pointer is found by the C++ core hit test.
package require Tk
package require tkd2dcanvas

tkd2dcanvas .c -width 640 -height 480 -background white
pack .c -fill both -expand 1

set item [.c create rectangle 40 40 160 120 -fill tomato -tags draggable]

# Drag state.
set ::dragItem ""
set ::dragX 0
set ::dragY 0

.c bind draggable <ButtonPress-1> {
    set ::dragItem [.c find withtag current]
    set ::dragX %x
    set ::dragY %y
}
.c bind draggable <B1-Motion> {
    .c move $::dragItem [expr {%x - $::dragX}] [expr {%y - $::dragY}]
    set ::dragX %x
    set ::dragY %y
}
.c bind draggable <ButtonRelease-1> {
    set ::dragItem ""
}
