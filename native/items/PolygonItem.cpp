#include "PolygonItem.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "tkd2dcanvas/HitTest.h"
#include "tkd2dcanvas/TkAreaOverlap.h"

namespace {

// The stored coordinate list Tk would hold: auto-closed like Tk
// (PolyCoords appends a copy of the first point when the list is open).
std::vector<Point> TestPoints(const std::vector<Point>& points,
                              const SmoothOptions& smooth) {
    std::vector<Point> tk = points;
    if (tk.size() >= 2 && tk.front() != tk.back()) {
        tk.push_back(tk.front());
    }
    if (smooth.mode != SmoothMode::None && tk.size() >= 4) {
        return SmoothCurvePoints(tk, smooth);
    }
    return tk;
}

}  // namespace

PolygonItem::PolygonItem(Id id, std::vector<Point> points)
    : CanvasItem(id, "polygon"), points_(std::move(points)) {
    // Tk defaults: black fill, no outline.
    Style().fill = Color{0, 0, 0};
}

BBox PolygonItem::GetBBox() const {
    if (IsHidden()) return BBox::Empty();
    // Tk: ComputePolygonBbox — an integer box over the (auto-closed) vertex
    // hull, padded by (int)((width + 1.5) / 2) when the polygon has an
    // outline, with miter apex points merged in for JoinMiter, plus a 1px
    // fudge. Smoothing never expands the box.
    const std::vector<Point> tk = TkPoints();  // auto-closes like Tk
    if (tk.empty()) return BBox::Empty();

    std::size_t n = tk.size();
    int x1 = static_cast<int>(tk[0].x), x2 = x1;
    int y1 = static_cast<int>(tk[0].y), y2 = y1;
    // Tk includes every stored point except the (duplicated) last one.
    for (std::size_t i = 1; i < n - 1; ++i) {
        const int ix = static_cast<int>(tk[i].x + 0.5);
        if (ix < x1) x1 = ix;
        if (ix > x2) x2 = ix;
        const int iy = static_cast<int>(tk[i].y + 0.5);
        if (iy < y1) y1 = iy;
        if (iy > y2) y2 = iy;
    }

    if (TkHasOutlineGC(Style())) {
        const double width = Style().width;
        const int pad = static_cast<int>((width + 1.5) / 2.0);
        x1 -= pad; x2 += pad; y1 -= pad; y2 += pad;

        if (Style().join == JoinStyle::Miter) {
            auto include = [&](const double miter[2], const double miter2[2]) {
                const int i1 = static_cast<int>(miter[0] + 0.5);
                if (i1 < x1) x1 = i1;
                if (i1 > x2) x2 = i1;
                const int i2 = static_cast<int>(miter[1] + 0.5);
                if (i2 < y1) y1 = i2;
                if (i2 > y2) y2 = i2;
                const int j1 = static_cast<int>(miter2[0] + 0.5);
                if (j1 < x1) x1 = j1;
                if (j1 > x2) x2 = j1;
                const int j2 = static_cast<int>(miter2[1] + 0.5);
                if (j2 < y1) y1 = j2;
                if (j2 > y2) y2 = j2;
            };
            // The closing seam joint (only when the stored list has more than 3 points).
            if (n > 3) {
                double miter1[2], miter2[2];
                if (TkGetMiterPoints(tk[n - 2], tk[0], tk[1], width,
                                     miter1, miter2)) {
                    include(miter1, miter2);
                }
            }
            // Interior joints of the closed loop.
            for (std::size_t k = 0; k + 2 < n; ++k) {
                double miter1[2], miter2[2];
                if (TkGetMiterPoints(tk[k], tk[k + 1], tk[k + 2], width,
                                     miter1, miter2)) {
                    include(miter1, miter2);
                }
            }
        }
    }

    x1 -= 1; x2 += 1; y1 -= 1; y2 += 1;
    return BBox{double(x1), double(y1), double(x2), double(y2)};
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

int PolygonItem::AreaOverlap(const double rect[4]) const {
    const std::vector<Point> store = TestPoints(points_, smooth_);
    if (store.size() < 2) return -1;

    const double width = Style().width;
    if (store.size() < 3) {
        const double radius = width / 2.0;
        const double oval[4] = {store[0].x - radius, store[0].y - radius,
                                store[0].x + radius, store[0].y + radius};
        return TkOvalToArea(oval, rect);
    }

    const int inside = TkPolygonToArea(store.data(), static_cast<int>(store.size()), rect);
    if (inside == 0) return 0;
    if (!TkHasOutlineGC(Style())) return inside;
    return TkPolygonOutlineToArea(store.data(), store.size(), width, Style().join, inside, rect);
}

bool PolygonItem::SetCoordinates(const std::vector<double>& coords) {
    if (coords.size() < 2 || coords.size() % 2 != 0) return false;
    std::vector<Point> points;
    points.reserve(coords.size() / 2);
    for (std::size_t i = 0; i < coords.size(); i += 2) {
        points.push_back(Point{coords[i], coords[i + 1]});
    }
    points_ = std::move(points);
    return true;
}

void PolygonItem::Draw(Renderer& renderer) const {
    if (IsHidden()) return;
    if (points_.empty()) return;

    Path path;
    // A two-point "polygon" is a line (Tk draws it as a single segment
    // with round joins at the ends). Stroke it as an open path so the
    // stroke can be snapped onto the pixel grid like create_line is;
    // AlignStrokePath's closed branch needs at least 3 points.
    path.closed = points_.size() >= 3;
    path.points = DisplayPoints();

    if (points_.size() >= 3) {
        if (const auto& fill = ItemStateFill(Style())) {
            renderer.fillPath(path, BrushStyle{*fill});
        }
    }
    if (const auto& outline = Style().outline) {
        PenStyle pen = ItemPenStyle(*outline, Style(), CapStyle::Round, Style().join);
        if (points_.size() < 3) {
            // The round ends of a two point polygon are joins (Tk draws the
            // polygon as a closed path), so they are not moved along the
            // segment: the round shape stays exactly where it was drawn.
            pen.capEnds = false;
        }
        renderer.drawLine(path, pen);
    }
}
