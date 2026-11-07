// test_view.cpp — unit tests for CanvasView (xview/yview state), following
// Tk's canvas semantics (tkCanvas.c: ScrollFractions / CanvasSetOrigin).
#include <cassert>
#include <cmath>
#include <cstdio>

#include "tkd2dcanvas/CanvasView.h"

static bool Near(double a, double b) { return std::fabs(a - b) < 1e-9; }

int main() {
    CanvasView view;
    view.SetViewSize(200, 100);

    // No scroll region: the full span is always visible.
    assert(view.XOffset() == 0.0 && view.YOffset() == 0.0);
    assert(Near(view.XViewFraction().first, 0.0) && Near(view.XViewFraction().last, 1.0));
    assert(Near(view.YViewFraction().first, 0.0) && Near(view.YViewFraction().last, 1.0));

    // A region enables scrolling; the offset starts at the region's origin.
    view.SetScrollRegion(BBox{0.0, 0.0, 1000.0, 500.0});
    assert(Near(view.XViewFraction().first, 0.0) && Near(view.XViewFraction().last, 0.2));
    assert(Near(view.YViewFraction().first, 0.0) && Near(view.YViewFraction().last, 0.2));

    // moveto <f> maps onto the *whole region width* (Tk), then confines.
    view.MoveTo(true, 0.5);
    assert(Near(view.XOffset(), 500.0));
    assert(Near(view.XViewFraction().first, 0.5) && Near(view.XViewFraction().last, 0.7));
    view.MoveTo(true, 1.0);   // pulled back by confine
    assert(Near(view.XOffset(), 800.0));
    view.MoveTo(true, -0.5);  // pulled back by confine
    assert(Near(view.XOffset(), 0.0));
    view.MoveTo(false, 1.0);  // vertical confine
    assert(Near(view.YOffset(), 400.0));

    // A region with a non-zero origin is reachable.
    view.SetScrollRegion(BBox{100.0, 50.0, 2100.0, 1050.0});
    view.SetOrigin(0.0, 0.0);  // pulled into the region
    assert(Near(view.XOffset(), 100.0) && Near(view.YOffset(), 50.0));
    assert(Near(view.XViewFraction().first, 0.0) && Near(view.XViewFraction().last, 0.1));
    assert(Near(view.YViewFraction().first, 0.0) && Near(view.YViewFraction().last, 0.1));

    // A region smaller than the window cannot be scrolled.
    view.SetScrollRegion(BBox{0.0, 0.0, 150.0, 80.0});
    view.MoveTo(true, 0.5);
    view.MoveTo(false, 0.5);
    assert(Near(view.XOffset(), 0.0) && Near(view.YOffset(), 0.0));
    assert(Near(view.XViewFraction().first, 0.0) && Near(view.XViewFraction().last, 1.0));

    // moveto with a huge fraction is pulled back by confine.
    view.SetScrollRegion(BBox{0.0, 0.0, 1000.0, 500.0});
    view.SetConfine(true);
    view.SetOrigin(2000.0, 0.0);
    assert(Near(view.XOffset(), 800.0));
    // ... and allowed when confine is off (Tk semantics).
    view.SetConfine(false);
    view.SetOrigin(2000.0, 0.0);
    assert(Near(view.XOffset(), 2000.0));
    view.SetConfine(true);

    // -xscrollincrement rounds the origin to a multiple of the increment.
    view.SetXScrollIncrement(100.0);
    view.SetOrigin(130.0, 0.0);
    assert(Near(view.XOffset(), 100.0));
    view.SetOrigin(150.0, 0.0);  // ties round away from zero
    assert(Near(view.XOffset(), 200.0));
    view.SetOrigin(-150.0, 0.0);
    assert(Near(view.XOffset(), -200.0));
    view.SetXScrollIncrement(0.0);  // restores unconstrained
    view.SetOrigin(130.0, 0.0);
    assert(Near(view.XOffset(), 130.0));

    // moveto + increment: the fraction lands on an increment boundary.
    view.SetXScrollIncrement(100.0);
    view.MoveTo(true, 0.13);  // 130 units -> 100
    assert(Near(view.XOffset(), 100.0));

    // scroll units: increment > 0 wins over one tenth of the span.
    view.SetXScrollIncrement(100.0);
    view.SetOrigin(0.0, 0.0);
    view.ScrollUnits(true, 2);
    assert(Near(view.XOffset(), 200.0));
    // ... otherwise one tenth of the visible width (20 canvas units).
    view.SetXScrollIncrement(0.0);
    view.SetOrigin(0.0, 0.0);
    view.ScrollUnits(true, 2);
    assert(Near(view.XOffset(), 40.0));
    // pages: nine tenths of the visible width.
    view.SetOrigin(0.0, 0.0);
    view.ScrollPages(true, 1);
    assert(Near(view.XOffset(), 180.0));
    view.ScrollPages(true, -1);
    assert(Near(view.XOffset(), 0.0));

    // y scrolling works independently of x.
    view.SetScrollRegion(BBox{0.0, 0.0, 1000.0, 500.0});
    view.SetYScrollIncrement(50.0);
    view.ScrollUnits(false, 1);
    assert(Near(view.XOffset(), 0.0) && Near(view.YOffset(), 50.0));

    // Resizing re-confines the view (Tk: ConfigureNotify recenters).
    view.SetScrollRegion(BBox{0.0, 0.0, 1000.0, 500.0});
    view.SetViewSize(200, 100);
    view.SetOrigin(800.0, 0.0);
    view.SetViewSize(500, 100);  // now only 500 units fit
    assert(Near(view.XOffset(), 500.0));

    // An empty region disables scrolling again (offset is not forced).
    view.SetOrigin(200.0, 0.0);
    view.SetScrollRegion(BBox::Empty());
    assert(Near(view.XViewFraction().first, 0.0) && Near(view.XViewFraction().last, 1.0));
    view.SetOrigin(123.0, 0.0);
    assert(Near(view.XOffset(), 123.0));

    view.SetViewSize(200, 100);
    std::puts("test_view: OK");
    return 0;
}
