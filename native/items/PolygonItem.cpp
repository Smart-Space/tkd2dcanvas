#include "PolygonItem.h"

#include <algorithm>

#include "tkd2dcanvas/HitTest.h"

PolygonItem::PolygonItem(Id id, std::vector<Point> points)
    : CanvasItem(id, "polygon"), points_(std::move(points)) {}

BBox PolygonItem::GetBBox() const {
    if (points_.empty()) return BBox::Empty();

    double minX = points_[0].x, maxX = points_[0].x;
    double minY = points_[0].y, maxY = points_[0].y;
    for (const auto& p : points_) {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
    }
    return BBox{minX, minY, maxX, maxY};
}

void PolygonItem::Translate(double dx, double dy) {
    for (auto& p : points_) { p.x += dx; p.y += dy; }
}

bool PolygonItem::HitTest(Point p, double tolerance) const {
    if (points_.size() < 3) return false;
    // Inside the fill, or close to an outline segment.
    if (HitTest::PointInPolygon(p, points_)) return true;
    for (std::size_t i = 0; i < points_.size(); ++i) {
        const Point& a = points_[i];
        const Point& b = points_[(i + 1) % points_.size()];
        if (HitTest::DistancePointToSegment(p, a, b) <= tolerance) return true;
    }
    return false;
}

void PolygonItem::Draw(Renderer&) const {
    // TODO(M4): emit a closed Path; fill + outline.
}
