#include "PolygonItem.h"

#include <algorithm>
#include <utility>

#include "tkd2dcanvas/HitTest.h"

PolygonItem::PolygonItem(Id id, std::vector<Point> points)
    : CanvasItem(id, "polygon"), points_(std::move(points)) {
    // Tk defaults: black fill, no outline.
    Style().fill = Color{0, 0, 0};
}

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

std::vector<double> PolygonItem::GetCoordinates() const {
    std::vector<double> coords;
    coords.reserve(points_.size() * 2);
    for (const auto& p : points_) {
        coords.push_back(p.x);
        coords.push_back(p.y);
    }
    return coords;
}

bool PolygonItem::SetCoordinates(const std::vector<double>& coords) {
    if (coords.size() < 6 || coords.size() % 2 != 0) return false;
    std::vector<Point> points;
    points.reserve(coords.size() / 2);
    for (std::size_t i = 0; i < coords.size(); i += 2) {
        points.push_back(Point{coords[i], coords[i + 1]});
    }
    points_ = std::move(points);
    return true;
}

void PolygonItem::Draw(Renderer& renderer) const {
    if (points_.size() < 3) return;

    Path path;
    path.closed = true;
    path.points = points_;

    if (const auto& fill = Style().fill) {
        renderer.fillPath(path, BrushStyle{*fill});
    }
    if (const auto& outline = Style().outline) {
        renderer.drawLine(path, ItemPenStyle(*outline, Style(), CapStyle::Round, Style().join));
    }
}
