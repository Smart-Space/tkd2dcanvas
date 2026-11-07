// test_hit_test.cpp — unit tests for hit-test geometry helpers.
#include <cassert>
#include <cstdio>
#include <cmath>
#include <vector>

#include "tkd2dcanvas/HitTest.h"

int main() {
    // Distance point -> segment
    assert(std::fabs(HitTest::DistancePointToSegment({0, 5}, {0, 0}, {10, 0}) - 5.0) < 1e-9);
    assert(std::fabs(HitTest::DistancePointToSegment({-3, -4}, {0, 0}, {10, 0}) - 5.0) < 1e-9);
    assert(HitTest::DistancePointToSegment({5, 0}, {0, 0}, {10, 0}) == 0.0);

    // Point in polygon (concave)
    const std::vector<Point> poly{{0, 0}, {10, 0}, {10, 10}, {5, 5}, {0, 10}};
    assert(HitTest::PointInPolygon({2, 2}, poly));
    // {5, 7} sits inside the V-notch between (10,10)-(5,5)-(0,10) → outside.
    assert(!HitTest::PointInPolygon({5, 7}, poly));
    assert(HitTest::PointInPolygon({2, 7}, poly));
    assert(!HitTest::PointInPolygon({12, 12}, poly));

    // Point in ellipse
    assert(HitTest::PointInEllipse({5, 5}, 5, 5, 4, 2));
    assert(!HitTest::PointInEllipse({9.5, 5}, 5, 5, 4, 2));

    // Point in rect with tolerance
    assert(HitTest::PointInRect({-0.5, 5}, 0, 0, 10, 10, 1.0));
    assert(!HitTest::PointInRect({-1.5, 5}, 0, 0, 10, 10, 1.0));

    std::puts("test_hit_test: OK");
    return 0;
}
