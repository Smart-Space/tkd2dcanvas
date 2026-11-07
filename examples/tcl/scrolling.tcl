# Scrolling demo (see examples/python/scrolling.py)
package require Tk
package require tkd2dcanvas

scrollbar .x -orient horizontal -command ".c xview"
scrollbar .y -orient vertical   -command ".c yview"

tkd2dcanvas .c -width 480 -height 320 -background white \
    -xscrollcommand ".x set" -yscrollcommand ".y set" \
    -scrollregion {0 0 1600 1200}

pack .y -side right -fill y
pack .x -side bottom -fill x
pack .c -side left -fill both -expand 1

foreach i {0 1 2 3 4 5} {
    set x [expr {40 + $i * 120}]
    set y [expr {40 + $i * 90}]
    set fill [expr {$i % 2 == 0 ? "#4f81bd" : "#c0504d"}]
    # M4: .c create rectangle $x $y [expr {$x + 120}] [expr {$y + 80}] -fill $fill
}
