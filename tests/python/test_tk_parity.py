"""Parity tests against the real Tk canvas (milestone M6).

The tag bindings of ``tkd2dcanvas`` have to behave exactly like the ones of
the Tk canvas: the same event types, the same hit test, the same dispatch
order.  Every test below runs the *same* Tcl scenario against both canvases
and compares the results, so a deviation shows up as a failing test.

The real Tk canvas is only available when tkinter can create a window; the
tests are skipped otherwise.
"""

import pytest

tkinter = pytest.importorskip("tkinter")

from tkd2dcanvas.errors import CommandError  # noqa: E402

# --- the scenarios -----------------------------------------------------------
# Each scenario is a Tcl script; "$c" is replaced by the canvas path.

# Probes the hit test: the tag binding fires exactly when the item under
# the pointer is the current item.
HIT_TEST = r"""
proc picked {c itemopts x y} {
    $c delete all
    set id [$c create {*}$itemopts]
    set ::hit 0
    $c bind t <Button-1> {set ::hit 1}
    catch {event generate $c <1> -x $x -y $y}
    catch {$c bind t <Button-1> {}}
    return $::hit
}
set out {}
foreach shape {
    {rect-fill   {rectangle 80 80 120 120 -fill blue -tags t}}
    {rect-nofill {rectangle 80 80 120 120 -tags t}}
    {oval-fill   {oval 80 80 120 120 -fill blue -tags t}}
    {oval-nofill {oval 80 80 120 120 -tags t}}
    {poly-fill   {polygon 80 80 120 80 120 120 80 120 -fill blue -tags t}}
    {poly-nofill {polygon 80 80 120 80 120 120 80 120 -tags t}}
    {line        {line 80 80 120 120 -fill black -tags t}}
    {arc-pie     {arc 80 80 120 120 -start 0 -extent 180 -fill blue -tags t}}
    {arc-plain   {arc 80 80 120 120 -start 0 -extent 180 -tags t}}
} {
    foreach pt {{100 100} {100 81} {100 79} {80 100} {78 100} {200 200}} {
        lappend out "[lindex $shape 0]@[lindex $pt 0],[lindex $pt 1]" \
            [picked $c [lindex $shape 1] [lindex $pt 0] [lindex $pt 1]]
    }
}
set out
"""

# The tolerance: how far outside of an item the pointer may be.
CLOSE_ENOUGH = r"""
proc picked {c itemopts x} {
    $c delete all
    set id [$c create {*}$itemopts]
    set ::hit 0
    $c bind t <Button-1> {set ::hit 1}
    catch {event generate $c <1> -x $x -y 100}
    catch {$c bind t <Button-1> {}}
    return $::hit
}
set out {}
foreach ce {1 2 4 8} {
    foreach w {0 1 4} {
        $c configure -closeenough $ce
        set key "ce$ce-w$w"
        set maxd -1
        for {set d 0} {$d <= 14} {incr d} {
            if {[picked $c [list rectangle 80 80 120 120 -fill blue -width $w -tags t] \
                    [expr {80 - $d}]]} {
                set maxd $d
            } else {
                break
            }
        }
        lappend out $key $maxd
    }
}
$c configure -closeenough 1
set out
"""

# "find closest" for a couple of shapes.
FIND_CLOSEST = r"""
set out {}
foreach shape {
    {rect-fill   {rectangle 80 80 120 120 -fill blue -tags t}}
    {rect-nofill {rectangle 80 80 120 120 -tags t}}
    {oval-nofill {oval 80 80 120 120 -tags t}}
    {poly-nofill {polygon 80 80 120 80 120 120 80 120 -tags t}}
    {line        {line 80 80 120 120 -fill black -tags t}}
} {
    $c delete all
    set id [$c create {*}[lindex $shape 1]]
    foreach pt {{100 100} {78 100} {200 200}} {
        lappend out "[lindex $shape 0]@[lindex $pt 0],[lindex $pt 1]" \
            [$c find closest [lindex $pt 0] [lindex $pt 1]]
    }
}
set out
"""

# Which event patterns the canvas accepts for a tag binding.
ACCEPTED_EVENTS = r"""
proc accepts {c seq} {
    if {[catch {$c bind t $seq {}} e]} { return "no" }
    catch {$c bind t $seq {}}
    return "yes"
}
set out {}
foreach seq {<Button-1> <Button-3> <Motion> <Enter> <Leave> <KeyPress-a> <Key>
              <FocusIn> <FocusOut> <Configure> <Map> <Destroy> <Visibility>
              <<Foo>>} {
    lappend out $seq [accepts $c $seq]
}
set out
"""

# The dispatch order: "all" first, then the tags of the item, then the item.
DISPATCH_ORDER = r"""
$c delete all
set id [$c create rectangle 80 80 120 120 -fill blue -tags {t1 t2}]
set ::hits {}
$c bind all <Button-1> {append ::hits {ALL}}
$c bind t1 <Button-1> {append ::hits {T1}}
$c bind t2 <Button-1> {append ::hits {T2}}
$c bind $id <Button-1> {append ::hits {ITEM}}
event generate $c <1> -x 100 -y 100
set out $::hits
"""

# Line bbox: the Tk "approximate bounding box" (integer canvas units, padded
# by (int)(width+0.5) plus a 1px fudge; smooth curves do not contribute).
LINE_BBOX = r"""
set out {}
foreach spec {
    {10 10 50 30}
    {0 20 100 20}
    {20 0 20 100}
    {10.4 10.4 50.6 30.6}
    {10.5 10.5 50.5 30.5}
    {-10.4 100 20 100}
    {10 10 50 30 -width 2}
    {10 10 50 30 -width 4.4}
    {10 10 50 30 -width 4.5}
    {10 10 50 30 -width 0}
    {10 50 40 10 90 80 120 30}
    {0 0 100 0 150 50 -width 8 -joinstyle miter}
    {0 0 100 0 10 20 -width 20 -joinstyle miter}
    {0 0 100 0 10 20 -width 20 -joinstyle round}
    {0 0 50 100 100 0 -smooth true}
    {30 40 30 40}
    {1.5 1.5 99.25 20.125 -width 3}
    {-50.9 100.0 10.0 0.1}
} {
    $c delete all
    set id [$c create line {*}$spec]
    lappend out "bbox=[join [$c bbox $id] ,]"
}
set out
"""

# moveto: the reference edge is the bbox origin of the first matching item.
LINE_MOVETO = r"""
proc f6 {L} {
    set out {}
    foreach v $L { lappend out [format %.6f $v] }
    return [join $out ,]
}
set out {}
$c create line 10 10 50 30 -tags g
$c create line 20 20 60 40 -tags g
set a [$c create line 10 10 50 30 -tags h]
set b [$c create line 20 20 60 40 -tags h]
$c moveto h 100 100
lappend out "sa=[f6 [$c coords $a]]" "sb=[f6 [$c coords $b]]"
$c delete all
set a [$c create line 10.7 10.2 50 30]
$c moveto $a 100 100
lappend out "sa=[f6 [$c coords $a]]" "ba=[join [$c bbox $a] ,]"
$c moveto $a "" 200
lappend out "sa=[f6 [$c coords $a]]" "ba=[join [$c bbox $a] ,]"
$c moveto $a 300 ""
lappend out "sa=[f6 [$c coords $a]]" "ba=[join [$c bbox $a] ,]"
$c delete all
$c create line 10 10 50 30 -tags m
$c create line 20 20 60 40 -tags m
$c moveto m 100 100
lappend out "bm=[join [$c bbox m] ,]"
$c moveto nosuchtag 0 0
lappend out "nomatch=ok"
set out
"""

# Rect/oval/polygon bbox vs Tk's integer box (round half away from zero,
# pen-width bloat, 1x1 minimum) — same scenario on both canvases.
SHAPES_BBOX = r"""
proc box {args} {
    $c delete all
    set id [$c create {*}$args]
    return "bbox=[join [$c bbox $id] ,]"
}
set out {}
foreach spec {{rectangle 10 10 100 80}
              {rectangle 10 10 100 80 -outline {}}
              {rectangle 10 10 100 80 -width 0}
              {rectangle 10 10 100 80 -width 3}
              {rectangle 10 10 100 80 -width 4.4}
              {rectangle 10 10 100 80 -width 4.5}
              {rectangle -10.6 -10.4 20.3 20.7}
              {rectangle 10.5 10.5 20.5 20.5}
              {rectangle 10 10 10 10}
              {rectangle 100 80 10 10}
              {oval 10 10 100 80}
              {oval 10 10 100 80 -outline {}}
              {oval 10 10 100 80 -width 4.4}
              {oval 10 10 10 10}
              {polygon 20 10 100 40 40 90}
              {polygon 20 10 100 40 40 90 -outline red}
              {polygon 20 10 100 40 40 90 -outline red -width 3}
              {polygon 20 10 100 40 40 90 -outline red -width 3 -joinstyle miter}
              {polygon 0 0 100 0 10 20 -outline red -width 20 -joinstyle miter}
              {polygon 0 0 150 50 100 0 -outline red -width 4 -joinstyle miter}
              {polygon 20 10 100 40 40 90 -smooth true}
              {polygon 10.5 10.4 90.6 40.5 40.5 90.4}
              {polygon 0 0 50 0 50 50 -outline red -width 10 -joinstyle bevel}
              {polygon 0 0 100 0 10 20 -outline red -width 20 -joinstyle round}} {
    lappend out [eval box $spec]
}
set out
"""

# find overlapping / enclosed for rect/oval/polygon: Tk uses the precise
# areaProc (unfilled items are hollow). Report "found in result" booleans so
# the scenario is immune to item-id drift.
SHAPES_AREA = r"""
set out {}
set shapes {
    {rectangle 10 10 60 60} {rectangle 10 10 60 60 -fill tomato} {rectangle 10 10 60 60 -width 4} {oval 10 10 60 60} {oval 10 10 60 60 -fill tomato} {oval 10 10 60 60 -width 4} {polygon 10 10 60 10 60 60 10 60} {polygon 10 10 60 10 60 60 10 60 -outline navy -width 3}
}
foreach spec $shapes {
    $c delete all
    set id [$c create {*}$spec]
    foreach probe {{0 0 70 70} {20 20 40 40} {55 55 100 100} {5 5 15 15} {0 0 9 70} {58 58 62 62} {30 25 40 35}} {
        lappend out "o[lsearch [$c find overlapping {*}$probe] $id]:e[lsearch [$c find enclosed {*}$probe] $id]"
    }
}
set out
"""

# Arc bbox + find: the arc's drawn extent box and areaProc (Tk 8.6.18).
ARC_AREA = r"""
set out {}
set specs {
    {pieslice 0 90} {pieslice 45 120} {pieslice 180 90} {pieslice 270 180}
    {pieslice 30 270} {pieslice 200 -120} {pieslice 135 240} {pieslice 0 180}
    {chord 0 90} {chord 45 120} {chord 200 -120} {chord 0 180}
    {arc 0 90} {arc 45 120} {arc 200 -120} {arc 0 180} {arc 135 240}
}
foreach spec $specs {
    foreach fill {none tomato} {
        foreach outline {navy none} {
            $c delete all
            set kw {}
            if {$fill ne "none"} { lappend kw -fill $fill }
            if {$outline ne "none"} { lappend kw -outline $outline } else { lappend kw -outline {} }
            set id [$c create arc 20 20 140 120 -style [lindex $spec 0] \
                -start [lindex $spec 1] -extent [lindex $spec 2] {*}$kw]
            lappend out "bbox=[join [$c bbox $id] ,]"
            foreach probe {{0 0 160 140} {80 20 140 70} {60 90 90 125} {30 30 45 45} {0 0 30 90} {138 20 142 125} {0 0 160 25}} {
                lappend out "o[lsearch [$c find overlapping {*}$probe] $id]:e[lsearch [$c find enclosed {*}$probe] $id]"
            }
        }
    }
}
set out
"""

# Unknown numeric item ids are silent no-ops (Tk semantics).
UNKNOWN_ID = r"""
set out {}
foreach cmd {{move 9999 1 1}
             {moveto 9999 10 10}
             {coords 9999}
             {bbox 9999}
             {itemcget 9999 -fill}
             {itemconfigure 9999 -width 3}
             {delete 9999}
             {raise 9999}
             {lower 9999}
             {find withtag 9999}
             {gettags 9999}
             {addtag g withtag 9999}
             {dtag 9999 g}
             {bind 9999 <Button-1> {puts x}}} {
    catch {eval "$c $cmd"} result
    lappend out "cmd=[lindex $cmd 0]:$result"
}
set out
"""

# find overlapping / enclosed on a line: Tk asks the item's precise areaProc,
# not the bbox (the bbox only acts as a coarse filter padded by 1px). The
# report is lsearch indices so the scenario is immune to item-id drift.
LINE_FIND_AREA = r"""
set out {}
$c delete all
set id [$c create line 20 20 100 20 -width 2 -tags t]
foreach probe {{0 0 100 100} {16 16 19 19} {17 22 17 23} {18 18 22 22}
               {0 0 17 17} {18 19 30 21} {0 0 21 21} {60 60 80 80}} {
    lappend out "o[lsearch [$c find overlapping {*}$probe] $id]" \
                "e[lsearch [$c find enclosed {*}$probe] $id]"
}
set out
"""

# A click that hits nothing does not dispatch at all.
NO_ITEM = r"""
$c delete all
set ::hits {}
$c bind all <Button-1> {append ::hits {ALL}}
event generate $c <1> -x 300 -y 300
set out $::hits
"""

# A tag that is added later is dispatched later.
ADDED_TAG = r"""
$c delete all
set id [$c create rectangle 80 80 120 120 -fill blue -tags {t1}]
set ::hits {}
$c bind t1 <Button-1> {append ::hits {T1}}
$c addtag t2 withtag $id
$c bind t2 <Button-1> {append ::hits {T2}}
event generate $c <1> -x 100 -y 100
set out $::hits
"""

# The widget level binding (Tk's "bind" command on the canvas path).
WIDGET_BINDING = r"""
$c delete all
set id [$c create rectangle 80 80 120 120 -fill blue -tags {t1}]
set ::hits {}
bind $c <Button-1> {append ::hits {WIDGET}}
$c bind t1 <Button-1> {append ::hits {TAG}}
event generate $c <1> -x 100 -y 100
set out $::hits
"""

# Several bindings for the same sequence: "+" appends, a plain script replaces.
MULTI_BINDING = r"""
$c delete all
set id [$c create rectangle 80 80 120 120 -fill blue -tags t]
set ::hits {}
$c bind t <Button-1> {append ::hits {A}}
$c bind t <Button-1> {+append ::hits {B}}
$c bind t <Button-1> {append ::hits {C}}
event generate $c <1> -x 100 -y 100
set out $::hits
"""


@pytest.fixture(scope="module")
def canvases():
    """A real Tk canvas and a tkd2dcanvas side by side."""
    try:
        root = tkinter.Tk()
    except tkinter.TclError as exc:  # pragma: no cover - depends on the environment
        pytest.skip(f"Tk unavailable in this environment: {exc}")

    from tkd2dcanvas import Canvas

    try:
        # Map the windows: Tk only delivers real Button events to mapped
        # windows (unmapped canvases swallow `<Button-1>`).
        root.deiconify()
        tk_canvas = tkinter.Canvas(root, width=320, height=240)
        tk_canvas.pack()
        root.update()

        try:
            our_canvas = Canvas(root, width=320, height=240)
        except CommandError as exc:
            pytest.skip(f"native package not built yet: {exc}")
        our_canvas.pack()
        root.update()
    except tkinter.TclError as exc:  # pragma: no cover
        pytest.skip(f"Tk unavailable in this environment: {exc}")

    yield root, str(tk_canvas), our_canvas.tk_path
    root.destroy()


def run(root, canvas_path, script):
    """Runs a scenario against one canvas and returns the flat result list."""
    result = root.tk.call("eval", script.replace("$c ", canvas_path + " ")
                          .replace("{$c", "{" + canvas_path))
    if isinstance(result, tuple):
        return list(result)
    return root.tk.splitlist(str(result))


def compare(canvases, script):
    """Runs a scenario against both canvases and compares the results."""
    root, tk_path, our_path = canvases
    tk_result = run(root, tk_path, script)
    our_result = run(root, our_path, script)
    assert our_result == tk_result


def test_hit_test_matches_tk(canvases):
    compare(canvases, HIT_TEST)


def test_close_enough_matches_tk(canvases):
    compare(canvases, CLOSE_ENOUGH)


def test_find_closest_matches_tk(canvases):
    compare(canvases, FIND_CLOSEST)


def test_accepted_event_types_match_tk(canvases):
    compare(canvases, ACCEPTED_EVENTS)


def test_dispatch_order_matches_tk(canvases):
    compare(canvases, DISPATCH_ORDER)


def test_no_item_matches_tk(canvases):
    compare(canvases, NO_ITEM)


def test_added_tag_order_matches_tk(canvases):
    compare(canvases, ADDED_TAG)


def test_widget_binding_matches_tk(canvases):
    compare(canvases, WIDGET_BINDING)


def test_multi_binding_matches_tk(canvases):
    compare(canvases, MULTI_BINDING)


def test_line_bbox_matches_tk(canvases):
    compare(canvases, LINE_BBOX)


def test_line_moveto_matches_tk(canvases):
    compare(canvases, LINE_MOVETO)


def test_line_find_area_matches_tk(canvases):
    compare(canvases, LINE_FIND_AREA)


def test_shapes_bbox_matches_tk(canvases):
    compare(canvases, SHAPES_BBOX)


def test_shapes_area_matches_tk(canvases):
    compare(canvases, SHAPES_AREA)


def test_arc_area_matches_tk(canvases):
    compare(canvases, ARC_AREA)


def test_unknown_id_matches_tk(canvases):
    compare(canvases, UNKNOWN_ID)
