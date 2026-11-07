# all.tcl — run all tkd2dcanvas Tcl tests via tcltest.
#
# Usage:
#   tclsh all.tcl          (or wish all.tcl)
#   python tools/run_tcl_tests.py   (falls back to Python's bundled Tk)

set dir [file dirname [file normalize [info script]]]
lappend auto_path [file join $dir .. .. tcl]
lappend auto_path [file join $dir .. .. python tkd2dcanvas tcl]

if {[catch {package require tcltest} err]} {
    puts stderr "tcltest not available: $err"
    exit 1
}

if {[catch {package require tkd2dcanvas} err]} {
    puts "SKIP: tkd2dcanvas package not built/loadable ($err)"
    puts "Build it first (see docs/building.md); milestone M0 = package loads."
    exit 0
}

# Mark that we are driving the run: the individual .test files must not call
# cleanupTests on their own (it would end the process after the first file).
set ::tkg_all_in_progress 1

foreach file [glob -nocomplain -dir $dir *.test] {
    puts "=== [file tail $file]"
    source $file
}

unset ::tkg_all_in_progress

if {[info exists ::tkg_tk_interp]} {
    # We run inside an already-running windowed interpreter (wish / tkinter).
    # cleanupTests prints the per-file summaries and then calls exit, which a
    # windowed interpreter reports as an error — catch it and keep the output.
    catch {tcltest::cleanupTests}
    puts "TCLTESTS-DONE"
} else {
    tcltest::cleanupTests
}
