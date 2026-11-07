#include "LineItem.h"

#include <algorithm>
#include <limits>

#include "tkd2dcanvas/HitTest.h"

LineItem::LineItem(Id id, std::vector<Point> points)
    : CanvasItem(id, "line"), points_(std::move(points)) {
    // Tk defaults: black 1px line. The paint channel for lines is -fill.
    Style().fill = Color{0, 0, 0};
}

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

std::vector<double> LineItem::GetCoordinates() const {
    std::vector<double> coords;
    coords.reserve(points_.size() * 2);
    for (const auto& p : points_) {
        coords.push_back(p.x);
        coords.push_back(p.y);
    }
    return coords;
}

bool LineItem::SetCoordinates(const std::vector<double>& coords) {
    if (coords.size() < 4 || coords.size() % 2 != 0) return false;
    std::vector<Point> points;
    points.reserve(coords.size() / 2);
    for (std::size_t i = 0; i < coords.size(); i += 2) {
        points.push_back(Point{coords[i], coords[i + 1]});
    }
    points_ = std::move(points);
    return true;
}

void LineItem::Draw(Renderer& renderer) const {
    if (points_.size() < 2) return;

    Path path;
    path.closed = false;
    path.points = points_;

    // Lines paint through -fill (Tk semantics); -outline is accepted as an
    // alias when -fill is empty.
    const std::optional<Color> paint = Style().fill.has_value() ? Style().fill : Style().outline;
    if (paint) {
        renderer.drawLine(path, ItemPenStyle(*paint, Style(), CapStyle::Round, JoinStyle::Round));
    }
}
