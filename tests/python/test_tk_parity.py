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
