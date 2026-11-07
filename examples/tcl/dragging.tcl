# Dragging demo (see examples/python/dragging.py)
package require Tk
package require tkd2dcanvas

tkd2dcanvas .c -width 640 -height 480 -background white
pack .c -fill both -expand 1

set item [.c create rectangle 40 40 160 120 -fill tomato -tags draggable]

# M6 will implement .c bind / tag_bind on the C++ side.
.c bind draggable <ButtonPress-1> { set ::drag_start [list %x %y] }
.c bind draggable <Motion>        {
    if {![info exists ::drag_start]} { return }
    set dx [expr {%x - [lindex $::drag_start 0]}]
    set dy [expr {%y - [lindex $::drag_start 1]}]
    .c move $item $dx $dy
    set ::drag_start [list %x %y]
}
.c bind draggable <ButtonRelease-1> { unset ::drag_start }
