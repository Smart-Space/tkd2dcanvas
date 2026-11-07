// HitTest.h — geometry helpers used by items and by CanvasModel for
// tag-based lookup / current item tracking.
#pragma once

#include <vector>
#include "tkd2dcanvas/Geometry.h"

namespace HitTest
{
    // Distance from p to segment ab (always >= 0).
    double DistancePointToSegment(Point p, Point a, Point b);

    // Even-odd point-in-polygon test. Works for concave polygons.
    bool PointInPolygon(Point p, const Point* points, std::size_t count);
    inline bool PointInPolygon(Point p, const std::vector<Point>& points) {
        return PointInPolygon(p, points.data(), points.size());
    }

    // |((px-cx)/rx)^2 + ((py-cy)/ry)^2| <= 1 containment test.
    bool PointInEllipse(Point p, double cx, double cy, double rx, double ry);

    // Point-to-rect containment with tolerance (for outlines).
    bool PointInRect(Point p, double left, double top, double right, double bottom, double tolerance);
}
