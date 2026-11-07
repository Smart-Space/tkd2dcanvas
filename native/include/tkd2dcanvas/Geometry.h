// Geometry.h — basic 2D geometry types used across core/items/render.
#pragma once

#include <vector>

struct Point {
    double x = 0.0;
    double y = 0.0;

    bool operator==(const Point& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Point& o) const { return !(*this == o); }
};

// A polyline path. `closed` joins the last point back to the first.
struct Path {
    std::vector<Point> points;
    bool closed = false;

    bool IsEmpty() const { return points.empty(); }
    std::size_t Size() const { return points.size(); }
    void Clear() { points.clear(); }
    void Add(Point p) { points.push_back(p); }
};
