#include "PolygonItem.h"

#include <algorithm>
#include <limits>
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

std::vector<Point> PolygonItem::TkPoints() const {
    if (points_.size() >= 2 && points_.front() != points_.back()) {
        std::vector<Point> closed = points_;
        closed.push_back(points_.front());
        return closed;
    }
    return points_;
}

std::vector<Point> PolygonItem::DisplayPoints() const {
    // Tk DisplayPolygon: the smoothed path is used only when the (possibly
    // auto-closed) coordinate list has at least 4 points.
    if (smooth_.mode != SmoothMode::None) {
        const std::vector<Point> tk = TkPoints();
        if (tk.size() >= 4) return SmoothCurvePoints(tk, smooth_);
    }
    return points_;
}

double PolygonItem::DistanceTo(Point p) const {
    // Tk hit-tests against the smoothed points (smooth && numPoints > 2).
    const std::vector<Point> tk = TkPoints();
    const std::vector<Point> poly = (smooth_.mode != SmoothMode::None && tk.size() > 2)
                                        ? SmoothCurvePoints(tk, smooth_)
                                        : points_;
    if (poly.size() < 3) return CanvasItem::DistanceTo(p);

    // Inside the polygon the distance is zero - like Tk, this does not depend
    // on whether the polygon is filled.
    if (HitTest::PointInPolygon(p, poly)) return 0.0;

    double best = std::numeric_limits<double>::max();
    for (std::size_t i = 0; i < poly.size(); ++i) {
        const Point& a = poly[i];
        const Point& b = poly[(i + 1) % poly.size()];
        best = std::min(best, HitTest::DistancePointToSegment(p, a, b));
    }
    return best;
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
    path.closed = true;  // closes back to the start of the spline loop
    path.points = DisplayPoints();

    if (const auto& fill = Style().fill) {
        renderer.fillPath(path, BrushStyle{*fill});
    }
    if (const auto& outline = Style().outline) {
        renderer.drawLine(path, ItemPenStyle(*outline, Style(), CapStyle::Round, Style().join));
    }
}
