#include "LineItem.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "tkd2dcanvas/HitTest.h"
#include "tkd2dcanvas/TkAreaOverlap.h"

namespace {

// --- Tk-compatible line bounding box ---------------------------------------
// tcl/tk computes the bounding box of a line in *integer* canvas units (the
// x1/y1/x2/y2 fields of the item header are ints) and the result is an
// "approximate bounding box ... may overestimate the actual bounding box by
// a few pixels" (see the canvas manual and ComputeLineBbox in tkCanvLine.c).
// tkd2dcanvas reproduces that algorithm exactly so that `bbox`,
// `moveto`, `find overlapping` and friends behave like the Tk canvas.
//
// Rounding rules, in Tk source order:
//   1. the first point is truncated towards zero (a bare C (int) cast);
//   2. every other point follows TkIncludePoint's (int)(p + 0.5) rule
//      (also truncated toward zero);
//   3. the box is padded by (int)(width + 0.5) on each side, where the
//      width is at least 1.0;
//   4. for -joinstyle miter the two miter apex points of every interior
//      vertex are added (TkGetMiterPoints in tkTrig.c, itself rounding with
//      floor(x + 0.5) and giving up on joints below 11 degrees);
//   5. one pixel of fudge is added on every side.
// The arrowhead pass of ComputeLineBbox is intentionally absent: line
// arrows are not implemented in tkd2dcanvas yet.

int TkTruncToInt(double value) {
    // A C cast of a double truncates toward zero (unlike std::floor/std::round).
    return static_cast<int>(value);
}

void TkIncludePoint(int& x1, int& y1, int& x2, int& y2, double px, double py) {
    const int ix = TkTruncToInt(px + 0.5);
    if (ix < x1) x1 = ix;
    if (ix > x2) x2 = ix;
    const int iy = TkTruncToInt(py + 0.5);
    if (iy < y1) y1 = iy;
    if (iy > y2) y2 = iy;
}

}  // namespace

LineItem::LineItem(Id id, std::vector<Point> points)
    : CanvasItem(id, "line"), points_(std::move(points)) {
    // Tk defaults: black 1px line. The paint channel for lines is -fill.
    Style().fill = Color{0, 0, 0};
}

BBox LineItem::GetBBox() const {
    if (points_.empty()) return BBox::Empty();

    int x1 = TkTruncToInt(points_[0].x), x2 = x1;
    int y1 = TkTruncToInt(points_[0].y), y2 = y1;
    for (std::size_t i = 1; i < points_.size(); ++i) {
        TkIncludePoint(x1, y1, x2, y2, points_[i].x, points_[i].y);
    }

    double width = Style().width;
    if (width < 1.0) width = 1.0;
    const int intWidth = TkTruncToInt(width + 0.5);
    x1 -= intWidth; x2 += intWidth;
    y1 -= intWidth; y2 += intWidth;

    if (points_.size() == 1) {
        x1 -= 1; x2 += 1;
        y1 -= 1; y2 += 1;
        return BBox{double(x1), double(y1), double(x2), double(y2)};
    }

    if (Style().join == JoinStyle::Miter) {
        for (std::size_t k = 0; k + 2 < points_.size(); ++k) {
            double miter1[2], miter2[2];
            if (TkGetMiterPoints(points_[k], points_[k + 1], points_[k + 2],
                                 width, miter1, miter2)) {
                TkIncludePoint(x1, y1, x2, y2, miter1[0], miter1[1]);
                TkIncludePoint(x1, y1, x2, y2, miter2[0], miter2[1]);
            }
        }
    }

    x1 -= 1; x2 += 1;
    y1 -= 1; y2 += 1;
    return BBox{double(x1), double(y1), double(x2), double(y2)};
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

int LineItem::AreaOverlap(const double rect[4]) const {
    // Tk's line areaProc (LineToArea in tkCanvLine.c). The smoothed path is
    // tested against the discrete spline points, exactly as for the hit test.
    const std::vector<Point> display = DisplayPoints();
    if (display.empty()) return -1;

    double width = Style().width;
    if (display.size() == 1) {
        const double radius = (width + 1.0) / 2.0;
        const double oval[4] = {display[0].x - radius, display[0].y - radius,
                                display[0].x + radius, display[0].y + radius};
        return TkOvalToArea(oval, rect);
    }
    if (width < 1.0) width = 1.0;
    // Line arrows are not implemented yet; the arrowhead area test of
    // LineToArea is intentionally absent (coincident with the bbox's).
    return TkThickPolyLineToArea(display.data(), display.size(), width,
                                 Style().cap, Style().join, rect);
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
