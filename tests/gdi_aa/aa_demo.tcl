# aa_demo.tcl --
#
# Renders the same scene twice: once with the plain (aliased) GDI
# rendering of Tk, and once with the manual anti-aliasing that the
# patched Tk implements in tk/win/tkWinAA.c.
#
# The two canvases are drawn with exactly the same commands, so any
# difference between them comes from the anti-aliasing.
#
# Usage:  wish86t.exe aa_demo.tcl
#
# NOTE: keep this file ASCII only.  wish reads the script with the system
# encoding, which is not UTF-8 on all Windows installations.

package require Tk

# Write errors to a log file instead of showing a dialog, so that the
# dialog cannot hide the canvases.
set ::logfile [file join [file dirname [info script]] aa_demo.log]
proc log {msg} {
    set f [open $::logfile a]
    puts $f $msg
    close $f
}
proc bgerror {msg} {
    log "BGERROR: $msg"
}

proc roundedRectPoints {x1 y1 x2 y2 r steps} {
    # Points that approximate a rounded rectangle.  The corners are
    # sampled from the arcs, so that the fill follows the rounded shape
    # instead of the sharp one.  The centre of each arc is the corner of
    # the rectangle inset by the radius.
    set pts {}
    foreach corner [list \
	    [list [expr {$x2-$r}] [expr {$y1+$r}] -90 0] \
	    [list [expr {$x2-$r}] [expr {$y2-$r}] 0 90] \
	    [list [expr {$x1+$r}] [expr {$y2-$r}] 90 180] \
	    [list [expr {$x1+$r}] [expr {$y1+$r}] 180 270]] {
	lassign $corner cx cy from to
	for {set i 0} {$i <= $steps} {incr i} {
	    set a [expr {$from + ($to - $from) * double($i) / $steps}]
	    set rad [expr {$a * acos(-1.0) / 180.0}]
	    lappend pts [expr {round($cx + $r * cos($rad))}] \
		    [expr {round($cy + $r * sin($rad))}]
	}
    }
    return $pts
}

proc drawScene {c} {
    $c delete all

    # A filled circle: the classic staircase edge of GDI.
    $c create oval 20 20 140 140 -fill "#e09090" -outline "#803030" -width 2

    # A diagonal line.
    $c create line 170 30 300 130 -fill "#204080" -width 3

    # A polygon with slanted edges.
    $c create polygon {330 20 420 40 400 140 320 110} \
        -fill "#90c0f0" -outline "#103060"

    # A pie slice.
    $c create arc 20 170 140 260 -start 30 -extent 140 -style pieslice \
        -fill "#ffe080" -outline "#806000" -width 2

    # Rounded corners, the cheap way: a rectangle drawn as a polygon with
    # a thick outline and round joins.  GDI rounds the corners of the
    # stroke, but the edge of the stroke is still jagged.
    $c create polygon {170 180 290 180 290 250 170 250} \
        -fill "" -outline "#309050" -width 12 -joinstyle round

    # Rounded corners, the accurate way: a polygon whose corners follow
    # the arcs.  The edges of the shape are curved, so they need the
    # anti-aliasing to look smooth.
    $c create polygon [roundedRectPoints 330 170 420 250 14 6] \
        -fill "#b0d0b0" -outline "#407050" -width 2

    # Text is not touched by the anti-aliasing: GDI already smooths
    # TrueType glyphs by itself.
    $c create text 200 265 -text "GDI text" -font TkDefaultFont \
        -fill "#404040"
}

set top .aa
tk::toplevel $top
wm title $top "TkGDI-AA-Demo"
wm geometry $top 900x330+120+120

ttk::frame $top.f -padding 8
pack $top.f -fill both -expand 1

ttk::label $top.f.l1 -text "plain GDI (aliased)" -anchor w
ttk::label $top.f.l2 -text "manual anti-aliasing" -anchor w
grid $top.f.l1 -row 0 -column 0 -sticky w
grid $top.f.l2 -row 0 -column 1 -sticky w

canvas $top.f.c1 -width 430 -height 260 -background white \
    -highlightthickness 0 -bd 0
canvas $top.f.c2 -width 430 -height 260 -background white \
    -highlightthickness 0 -bd 0 -antialias 1 -aafactor 4
grid $top.f.c1 -row 1 -column 0 -sticky nsew
grid $top.f.c2 -row 1 -column 1 -sticky nsew
grid rowconfigure $top.f 1 -weight 1
grid columnconfigure $top.f 0 -weight 1
grid columnconfigure $top.f 1 -weight 1

drawScene $top.f.c1
drawScene $top.f.c2

update idletasks
update

foreach c [list $top.f.c1 $top.f.c2] {
    log [format "%s: x=%s y=%s w=%s h=%s items=%d aa=%s" $c \
	    [winfo x $c] [winfo y $c] [winfo width $c] [winfo height $c] \
	    [llength [$c find all]] [$c cget -antialias]]
}

# Keep the window open; the capture tool takes the screenshot.
after 60000 {exit 0}
