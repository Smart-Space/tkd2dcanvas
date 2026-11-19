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

    # A rounded shape, assembled from arcs and rectangles.
    $c create rectangle 170 170 290 250 -fill "#b0d0b0" -outline "" \
        -tags corner
    $c create arc 150 170 210 230 -start 90 -extent 90 -style pieslice \
        -fill "#b0d0b0" -outline "" -tags corner
    $c create arc 250 170 310 230 -start 0 -extent 90 -style pieslice \
        -fill "#b0d0b0" -outline "" -tags corner
    $c create arc 150 210 210 270 -start 180 -extent 90 -style pieslice \
        -fill "#b0d0b0" -outline "" -tags corner
    $c create arc 250 210 310 270 -start 270 -extent 90 -style pieslice \
        -fill "#b0d0b0" -outline "" -tags corner

    # Text is not touched by the anti-aliasing: GDI already smooths
    # TrueType glyphs by itself.
    $c create text 350 210 -text "GDI text" -font TkDefaultFont \
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
