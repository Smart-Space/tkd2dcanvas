#include "LineItem.h"

#include <algorithm>
#include <cmath>
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

std::vector<Point> LineItem::DisplayPoints() const {
    // Tk: smoothing applies only with more than 2 points.
    if (smooth_.mode != SmoothMode::None && points_.size() > 2) {
        return SmoothCurvePoints(points_, smooth_);
    }
    return points_;
}

double LineItem::DistanceTo(Point p) const {
    const std::vector<Point> display = DisplayPoints();
    if (display.empty()) return CanvasItem::DistanceTo(p);
    if (display.size() == 1) {
        const double dx = p.x - display[0].x;
        const double dy = p.y - display[0].y;
        return std::sqrt(dx * dx + dy * dy);
    }

    double best = std::numeric_limits<double>::max();
    for (std::size_t i = 0; i + 1 < display.size(); ++i) {
        best = std::min(best, HitTest::DistancePointToSegment(p, display[i], display[i + 1]));
    }
    return best;
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
    path.points = DisplayPoints();

    // Lines paint through -fill (Tk semantics); -outline is accepted as an
    // alias when -fill is empty.
    const std::optional<Color> paint = Style().fill.has_value() ? Style().fill : Style().outline;
    if (paint) {
        renderer.drawLine(path, ItemPenStyle(*paint, Style(), Style().cap, Style().join));
    }
}
