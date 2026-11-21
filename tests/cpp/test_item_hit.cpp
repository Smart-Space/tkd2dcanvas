// test_item_hit.cpp — unit tests for the hit test of the concrete items.
//
// The rules follow Tk:
//   * a filled shape is hit everywhere inside it,
//   * an unfilled shape is only hit near its outline,
//   * the outline width widens the hit area by half its width.
#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>

#include "tkd2dcanvas/CanvasModel.h"

#include "ArcItem.h"
#include "LineItem.h"
#include "OvalItem.h"
#include "PolygonItem.h"
#include "RectItem.h"

namespace {

bool Near(double a, double b) {
    return std::fabs(a - b) < 1e-9;
}

}  // namespace

int main() {
    // --- rectangle ----------------------------------------------------------
    RectItem rect(0, 0, 0, 100, 100);
    rect.Style().fill = Color{0, 0, 255};

    // Filled: the whole interior is a hit.
    assert(Near(rect.DistanceTo(Point{50, 50}), 0.0));
    assert(rect.HitTest(Point{50, 50}, 0.0));
    assert(rect.HitTest(Point{99, 99}, 0.0));
    // Outside: the distance to the rectangle.
    assert(Near(rect.DistanceTo(Point{110, 50}), 10.0));
    assert(!rect.HitTest(Point{110, 50}, 0.0));
    assert(rect.HitTest(Point{110, 50}, 10.0));

    // Unfilled: only the outline counts.
    rect.Style().fill.reset();
    assert(Near(rect.DistanceTo(Point{50, 50}), 50.0));   // to the nearest edge
    assert(!rect.HitTest(Point{50, 50}, 1.0));
    assert(rect.HitTest(Point{0, 50}, 1.0));           // on the outline
    assert(rect.HitTest(Point{2, 50}, 1.0));

    // The outline width widens the hit area by half its width.
    rect.Style().width = 4.0;
    assert(rect.HitTest(Point{50, 50}, 50.0 - 2.0));
    assert(!rect.HitTest(Point{50, 50}, 50.0 - 3.0));

    // No fill and no outline (-width 0): Tk hits the whole interior.  This is
    // what TinUI's radiobox uses as its invisible hit rectangle; treating it
    // as hollow would make its options respond only along the edge.
    rect.Style().width = 0.0;
    assert(Near(rect.DistanceTo(Point{50, 50}), 0.0));
    assert(rect.HitTest(Point{50, 50}, 0.0));
    assert(rect.HitTest(Point{50, 25}, 0.0));
    assert(!rect.HitTest(Point{110, 50}, 0.0));

    // --- oval ---------------------------------------------------------------
    OvalItem oval(0, 0, 0, 100, 100);
    oval.Style().fill = Color{0, 0, 255};
    assert(Near(oval.DistanceTo(Point{50, 50}), 0.0));
    assert(oval.HitTest(Point{50, 50}, 0.0));
    // Outside the ellipse (the corner of the bounding box is far away).
    assert(oval.DistanceTo(Point{0, 0}) > 40.0);
    assert(!oval.HitTest(Point{0, 0}, 1.0));

    oval.Style().fill.reset();
    assert(oval.DistanceTo(Point{50, 50}) > 0.0);      // the centre is far from the outline
    assert(!oval.HitTest(Point{50, 50}, 1.0));
    assert(oval.HitTest(Point{50, 0}, 1.0));           // on the outline

    // --- line ---------------------------------------------------------------
    LineItem line(0, {Point{0, 0}, Point{100, 0}});
    assert(Near(line.DistanceTo(Point{50, 0}), 0.0));
    assert(line.HitTest(Point{50, 0}, 0.0));
    assert(Near(line.DistanceTo(Point{50, 10}), 10.0));
    assert(!line.HitTest(Point{50, 10}, 1.0));
    assert(line.HitTest(Point{50, 10}, 10.0));

    // --- polygon ------------------------------------------------------------
    PolygonItem polygon(0, {Point{0, 0}, Point{100, 0}, Point{100, 100}, Point{0, 100}});
    // Inside a polygon counts as a hit - with or without a fill (Tk behaves
    // the same way).
    assert(Near(polygon.DistanceTo(Point{50, 50}), 0.0));
    assert(polygon.HitTest(Point{50, 50}, 0.0));
    polygon.Style().fill.reset();
    assert(Near(polygon.DistanceTo(Point{50, 50}), 0.0));
    assert(polygon.HitTest(Point{50, 50}, 0.0));
    assert(Near(polygon.DistanceTo(Point{150, 50}), 50.0));

    // --- arc ----------------------------------------------------------------
    ArcItem arc(0, 0, 0, 100, 100, 0.0, 180.0);   // the upper half
    arc.Style().fill = Color{0, 0, 255};
    // The centre is on the border of the pie slice.
    assert(arc.DistanceTo(Point{50, 50}) >= 0.0);
    assert((arc.HitTest(Point{50, 10}, 0.0)));      // inside the filled slice
    arc.Style().fill.reset();
    assert(!arc.HitTest(Point{50, 10}, 1.0));     // not filled: only the outline
    assert(arc.HitTest(Point{50, 0}, 1.0));       // on the arc itself

    // --- the model uses the same hit test ------------------------------------
    CanvasModel model;
    const int id = model.CreateItem(std::make_unique<RectItem>(0, 0, 0, 100, 100));
    RectItem& item = static_cast<RectItem&>(*model.FindItem(id));
    item.Style().fill = Color{0, 0, 255};
    assert(model.FindItemAt(50, 50, 0.0) == id);
    assert(model.FindItemAt(110, 50, 0.0) == CanvasModel::kInvalidId);
    assert(model.FindItemAt(110, 50, 10.0) == id);
    item.Style().fill.reset();
    assert(model.FindItemAt(50, 50, 0.0) == CanvasModel::kInvalidId);
    assert(model.FindItemAt(0, 50, 0.0) == id);

    std::puts("test_item_hit: OK");
    return 0;
}
