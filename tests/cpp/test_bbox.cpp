// test_bbox.cpp — unit tests for BBox.
#include <cassert>
#include <cstdio>

#include "tkd2dcanvas/BBox.h"

int main() {
    const BBox empty = BBox::Empty();
    assert(!empty.IsValid());

    const BBox a{0, 0, 10, 10};
    assert(a.IsValid());
    assert(a.Width() == 10.0 && a.Height() == 10.0);

    const BBox b{20, 20, 30, 30};
    assert(!a.Intersects(b));
    assert(a.Union(b).left == 0.0 && a.Union(b).right == 30.0);
    assert(a.Union(empty) == a);

    const BBox c{5, 5, 15, 15};
    assert(a.Intersects(c));

    BBox d{0, 0, 10, 10};
    d.Inflate(5, 5);
    assert(d.left == -5.0 && d.top == -5.0 && d.right == 15.0 && d.bottom == 15.0);

    std::puts("test_bbox: OK");
    return 0;
}
