#include "tkd2dcanvas/HitTest.h"

#include <cmath>
#include <algorithm>

namespace HitTest
{

double DistancePointToSegment(Point p, Point a, Point b) {
    const double abx = b.x - a.x;
    const double aby = b.y - a.y;
    const double apx = p.x - a.x;
    const double apy = p.y - a.y;

    const double lenSq = abx * abx + aby * aby;
    double t = 0.0;
    if (lenSq > 0.0) {
        t = (apx * abx + apy * aby) / lenSq;
        t = std::clamp(t, 0.0, 1.0);
    }
    const double cx = a.x + t * abx;
    const double cy = a.y + t * aby;
    const double dx = p.x - cx;
    const double dy = p.y - cy;
    return std::sqrt(dx * dx + dy * dy);
}

bool PointInPolygon(Point p, const Point* points, std::size_t count) {
    if (points == nullptr || count < 3) return false;

    bool inside = false;
    for (std::size_t i = 0, j = count - 1; i < count; j = i++) {
        const double xi = points[i].x, yi = points[i].y;
        const double xj = points[j].x, yj = points[j].y;
        const bool intersects = ((yi > p.y) != (yj > p.y)) &&
            (p.x < (xj - xi) * (p.y - yi) / (yj - yi) + xi);
        if (intersects) inside = !inside;
    }
    return inside;
}

bool PointInEllipse(Point p, double cx, double cy, double rx, double ry) {
    if (rx <= 0.0 || ry <= 0.0) return false;
    const double dx = (p.x - cx) / rx;
    const double dy = (p.y - cy) / ry;
    return dx * dx + dy * dy <= 1.0;
}

bool PointInRect(Point p, double left, double top, double right, double bottom, double tolerance) {
    return p.x >= left - tolerance && p.x <= right + tolerance &&
           p.y >= top - tolerance && p.y <= bottom + tolerance;
}

}  // namespace HitTest
