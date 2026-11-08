// test_window_item.cpp — unit tests for the window item's model geometry
// (bbox, scaling, state handling). The Tk-side sync logic is exercised by the
// Tcl/Python suites.
#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>

#include "WindowItem.h"

namespace {

bool Near(double a, double b) {
    return std::fabs(a - b) < 1e-9;
}

bool BoxNear(const BBox& box, double l, double t, double r, double b) {
    return Near(box.left, l) && Near(box.top, t) && Near(box.right, r) && Near(box.bottom, b);
}

}  // namespace

int main() {
    // --- no window assigned yet: Tk's 1x1 bbox at the rounded position ------
    {
        WindowItem item(1, 10.4, 20.6);
        const BBox box = item.GetBBox();
        assert(BoxNear(box, 10.0, 21.0, 11.0, 22.0));
    }

    // --- natural size from the embedded window -------------------------------
    {
        WindowItem item(1, 50.0, 60.0, ".b");
        item.SetNaturalSize(94, 30);   // glue reports the requested size
        // Tk anchor default "center".
        assert(BoxNear(item.GetBBox(), 3.0, 45.0, 97.0, 75.0));
        item.SetNaturalSize(170, 30);  // e.g. the button's text grew
        assert(BoxNear(item.GetBBox(), -35.0, 45.0, 135.0, 75.0));
    }

    // --- requested size overrides the natural size ----------------------------
    {
        WindowItem item(1, 50.0, 60.0, ".b");
        item.SetNaturalSize(94, 30);
        item.SetRequestedSize(200, 80);
        assert(BoxNear(item.GetBBox(), -50.0, 20.0, 150.0, 100.0));
        assert(item.EffectiveWidth() == 200 && item.EffectiveHeight() == 80);
        item.SetRequestedSize(0, 0);   // 0 = natural (Tk)
        assert(BoxNear(item.GetBBox(), 3.0, 45.0, 97.0, 75.0));
    }

    // --- anchor placement ------------------------------------------------------
    {
        WindowItem item(1, 100.0, 100.0, ".b");
        item.SetNaturalSize(40, 20);
        item.SetAnchor("nw");
        assert(BoxNear(item.GetBBox(), 100.0, 100.0, 140.0, 120.0));
        item.SetAnchor("se");
        assert(BoxNear(item.GetBBox(), 60.0, 80.0, 100.0, 100.0));
        item.SetAnchor("n");
        assert(BoxNear(item.GetBBox(), 80.0, 100.0, 120.0, 120.0));
        item.SetAnchor("e");
        assert(BoxNear(item.GetBBox(), 60.0, 90.0, 100.0, 110.0));
        item.SetAnchor("center");
        assert(BoxNear(item.GetBBox(), 80.0, 90.0, 120.0, 110.0));
    }

    // --- hidden state: empty bbox like the other items ------------------------
    {
        WindowItem item(1, 10.0, 10.0, ".b");
        item.SetNaturalSize(40, 20);
        assert(item.GetBBox().IsValid());
        ItemStyle& style = item.Style();
        style.state = ItemState::Hidden;
        assert(!item.GetBBox().IsValid());
        style.state = ItemState::Normal;
        assert(item.GetBBox().IsValid());
    }

    // --- window removed while item visible: back to 1x1 ------------------------
    {
        WindowItem item(1, 33.2, 44.7, ".b");
        item.SetNaturalSize(40, 20);
        item.SetWindowPath("");  // embedded window destroyed
        assert(BoxNear(item.GetBBox(), 33.0, 45.0, 34.0, 46.0));
    }

    // --- translate / scale -----------------------------------------------------
    {
        WindowItem item(1, 10.0, 20.0, ".b");
        item.SetNaturalSize(40, 20);
        item.Translate(5.0, -5.0);
        assert(Near(item.X(), 15.0) && Near(item.Y(), 15.0));
        item.Scale(0.0, 0.0, 2.0, 0.5);
        assert(Near(item.X(), 30.0) && Near(item.Y(), 7.5));
        // Natural size is not scaled (it follows the embedded window).
        assert(item.EffectiveWidth() == 40 && item.EffectiveHeight() == 20);
        item.SetRequestedSize(10, 10);
        item.Scale(0.0, 0.0, 3.0, 4.0);
        assert(item.RequestedWidth() == 30 && item.RequestedHeight() == 40);
    }

    // --- coordinates ------------------------------------------------------------
    {
        WindowItem item(1, 10.0, 20.0, ".b");
        const std::vector<double> coords = item.GetCoordinates();
        assert(coords.size() == 2 && Near(coords[0], 10.0) && Near(coords[1], 20.0));
        assert(item.SetCoordinates({100.5, 200.25}));
        assert(Near(item.X(), 100.5) && Near(item.Y(), 200.25));
        assert(!item.SetCoordinates({1.0}));  // wrong arity
        assert(!item.SetCoordinates({1.0, 2.0, 3.0}));
    }

    // --- DistanceTo / hit-testing stay bbox based -------------------------------
    {
        WindowItem item(1, 0.0, 0.0, ".b");
        item.SetNaturalSize(100, 100);
        assert(Near(item.DistanceTo(Point{50, 50}), 0.0));
        assert(item.HitTest(Point{50, 50}, 0.0));
        assert(Near(item.DistanceTo(Point{130, 50}), 30.0));
    }

    // --- AreaOverlap ----------------------------------------------------------
    {
        WindowItem item(1, 0.0, 0.0, ".b");
        item.SetNaturalSize(100, 100);
        const double rect[4] = {50.0, 50.0, 200.0, 200.0};
        assert(item.AreaOverlap(rect) == 0);   // overlaps
        const double inside[4] = {-100.0, -100.0, 200.0, 200.0};
        assert(item.AreaOverlap(inside) == 1); // entirely inside
        const double outside[4] = {500.0, 500.0, 600.0, 600.0};
        assert(item.AreaOverlap(outside) == -1);
    }

    // --- Scale with negative factors mirrors the point ---------------------------
    {
        WindowItem item(1, 10.0, 10.0, ".b");
        item.SetNaturalSize(10, 10);
        item.Scale(0.0, 0.0, -1.0, -1.0);
        assert(Near(item.X(), -10.0) && Near(item.Y(), -10.0));
    }

    std::printf("test_window_item: all passed\n");
    return 0;
}
