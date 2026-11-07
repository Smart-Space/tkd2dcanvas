#include "LineItem.h"

#include <algorithm>
#include <limits>

#include "tkd2dcanvas/HitTest.h"

LineItem::LineItem(Id id, std::vector<Point> points)
    : CanvasItem(id, "line"), points_(std::move(points)) {}

BBox LineItem::GetBBox() const {
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

void LineItem::Translate(double dx, double dy) {
    for (auto& p : points_) { p.x += dx; p.y += dy; }
}

bool LineItem::HitTest(Point p, double tolerance) const {
    if (points_.size() < 2) return false;
    for (std::size_t i = 0; i + 1 < points_.size(); ++i) {
        if (HitTest::DistancePointToSegment(p, points_[i], points_[i + 1]) <= tolerance) return true;
    }
    return false;
}

void LineItem::Draw(Renderer&) const {
    // TODO(M4): emit a Path with moveTo/lineTo segments.
}
