// test_scale.cpp — unit tests for CanvasItem::Scale (the coordinate
// transform shared by `scale`) and for the fixed coordinate order that
// rectangles/ovals/arcs store their coordinates in.
#include <cassert>
#include <cmath>
#include <memory>
#include <vector>

#include "tkd2dcanvas/CanvasModel.h"

#include "ArcItem.h"
#include "LineItem.h"
#include "OvalItem.h"
#include "PolygonItem.h"
#include "RectItem.h"

namespace {

bool Near(double a, double b) { return std::fabs(a - b) < 1e-9; }

bool CoordsEq(const std::vector<double>& coords, const std::vector<double>& want) {
    if (coords.size() != want.size()) return false;
    for (std::size_t i = 0; i < coords.size(); ++i) {
        if (!Near(coords[i], want[i])) return false;
    }
    return true;
}

}  // namespace

int main() {
    // --- rectangles ----------------------------------------------------------
    RectItem rect(1, 10, 20, 110, 80);
    rect.Scale(0.0, 0.0, 2.0, 3.0);
    assert(CoordsEq(rect.GetCoordinates(), {20, 60, 220, 240}));

    // Negative factors mirror the box; Tk stores fixed (left, top, right,
    // bottom) order.
    rect.Scale(100.0, 100.0, -1.0, -1.0);
    assert(CoordsEq(rect.GetCoordinates(), {-20, -40, 180, 140}));

    // Reversed creation is normalized too.
    RectItem reversed(2, 110, 80, 10, 20);
    assert(CoordsEq(reversed.GetCoordinates(), {10, 20, 110, 80}));

    // --- ovals ----------------------------------------------------------------
    OvalItem oval(3, 110, 80, 10, 20);
    assert(CoordsEq(oval.GetCoordinates(), {10, 20, 110, 80}));
    oval.Scale(60.0, 50.0, 2.0, 2.0);
    assert(CoordsEq(oval.GetCoordinates(), {-40, -10, 160, 110}));

    // --- arcs (angles are untouched by the transform) -------------------------
    ArcItem arc(4, 10, 10, 110, 90, 30.0, 120.0);
    arc.Scale(60.0, 50.0, 2.0, 2.0);
    assert(CoordsEq(arc.GetCoordinates(), {-40, -30, 160, 130}));
    assert(Near(arc.Start(), 30.0));
    assert(Near(arc.Extent(), 120.0));

    // --- line and polygon (raw point pairs, order kept) -------------------------
    LineItem line(5, {{10, 10}, {100, 50}});
    line.Scale(0.0, 0.0, 0.5, 2.0);
    assert(CoordsEq(line.GetCoordinates(), {5, 20, 50, 100}));

    PolygonItem poly(6, {{0, 0}, {40, 0}, {20, 30}});
    poly.Scale(10.0, 10.0, 2.0, 2.0);
    assert(CoordsEq(poly.GetCoordinates(), {-10, -10, 70, -10, 30, 50}));

    return 0;
}
