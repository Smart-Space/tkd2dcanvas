// test_geometry.cpp — unit tests for Path and Transform2D.
#include <cassert>
#include <cstdio>
#include <cmath>

#include "tkd2dcanvas/Geometry.h"
#include "tkd2dcanvas/Transform2D.h"

static bool Near(double a, double b) { return std::fabs(a - b) < 1e-9; }

int main() {
    Path path;
    assert(path.IsEmpty());
    path.Add({0, 0});
    path.Add({10, 0});
    assert(path.Size() == 2 && !path.closed);

    const Transform2D t = Transform2D::Translate(10.0, 20.0) * Transform2D::Scale(2.0, 2.0);
    const Point p = t.Apply(Point{1.0, 1.0});
    // Scale first (rhs), then translate: (1,1) -> (2,2) -> (12,22)
    assert(Near(p.x, 12.0) && Near(p.y, 22.0));

    const Transform2D id = Transform2D::Identity();
    assert(id.IsIdentity());
    assert(!t.IsIdentity());

    const BBox box{0, 0, 10, 10};
    const BBox moved = Transform2D::Translate(5, 7).Apply(box);
    assert(Near(moved.left, 5.0) && Near(moved.top, 7.0) &&
           Near(moved.right, 15.0) && Near(moved.bottom, 17.0));

    std::puts("test_geometry: OK");
    return 0;
}
