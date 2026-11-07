# Optional Tcl-side helpers for tkd2dcanvas.
# The widget itself is implemented fully in C++; this file exists for
# convenience commands that do not belong in C.

namespace eval ::tkd2dcanvas {
    namespace export demo version

    proc version {} {
        return [package present tkd2dcanvas]
    }

    # Minimal smoke demo: a canvas with one rectangle.
    proc demo {parent} {
        if {![winfo exists $parent]} {
            error "demo window $parent does not exist"
        }
        set c "${parent}.democanvas"
        if {![winfo exists $c]} {
            tkd2dcanvas $c -width 320 -height 240 -background white
            pack $c -fill both -expand 1
        }
        return $c
    }
}
