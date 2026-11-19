# aa_diag.tcl --
#
# Smoke test for the manual anti-aliasing of the Tk canvas: draws one
# scene on a plain canvas and on a canvas with "-antialias 1", and writes
# the geometry of both canvases to a log file.
#
# Usage:  wish86t.exe aa_diag.tcl
#
# NOTE: keep this file ASCII only.  wish reads the script with the system
# encoding, which is not UTF-8 on all Windows installations.

package require Tk

set ::logfile [file join [file dirname [info script]] aa_diag.log]
proc log {msg} {
    set f [open $::logfile a]
    puts $f $msg
    close $f
}
proc bgerror {msg} {
    log "BGERROR: $msg"
}

proc drawScene {c} {
    $c delete all
    $c create oval 20 20 140 140 -fill "#e09090" -outline "#803030" -width 2
    $c create line 170 30 300 130 -fill "#204080" -width 3
    $c create polygon {330 20 420 40 400 140 320 110} \
        -fill "#90c0f0" -outline "#103060"
    $c create arc 20 170 140 260 -start 30 -extent 140 -style pieslice \
        -fill "#ffe080" -outline "#806000" -width 2
    $c create rectangle 170 170 290 250 -fill "#b0d0b0" -outline ""
    $c create text 350 210 -text "GDI text" -font TkDefaultFont -fill "#404040"
}

wm title . "TkGDI-AA-Diag"
wm geometry . 520x330+200+200

canvas .c1 -width 480 -height 300 -background white \
    -highlightthickness 0 -bd 0
canvas .c2 -width 480 -height 300 -background white \
    -highlightthickness 0 -bd 0 -antialias 1 -aafactor 4
pack .c1 -side left -fill both -expand 1
pack .c2 -side left -fill both -expand 1

drawScene .c1
drawScene .c2

update idletasks
update

foreach c [list .c1 .c2] {
    log [format "%s: x=%s y=%s w=%s h=%s items=%d aa=%s" $c \
	    [winfo x $c] [winfo y $c] [winfo width $c] [winfo height $c] \
	    [llength [$c find all]] [$c cget -antialias]]
}

after 30000 {exit 0}
