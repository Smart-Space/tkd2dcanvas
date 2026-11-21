#include "ArcItem.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <limits>
#include <string>
#include <utility>

#include "tkd2dcanvas/HitTest.h"
#include "tkd2dcanvas/TkAreaOverlap.h"

namespace {
constexpr double kDegToRad = 3.14159265358979323846 / 180.0;

std::string ToLower(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (unsigned char c : text) out.push_back(static_cast<char>(std::tolower(c)));
    return out;
}

// Tk keeps the raw coordinates normalized (min/max) in the item header.
struct ArcGeometry {
    double x1, y1, x2, y2;          // normalized bbox
    double boxWidth, boxHeight;
    double centerX, centerY;
    double c1x, c1y;                // Tk's center1: the arc's start endpoint
    double c2x, c2y;                // Tk's center2: the arc's end endpoint
    double sin1, cos1, sin2, cos2;  // endpoint angles (negated, y-down)
};

ArcGeometry ComputeArcGeometry(double rx1, double ry1, double rx2, double ry2,
                               double startDeg, double extentDeg) {
    ArcGeometry g;
    g.x1 = std::min(rx1, rx2);
    g.y1 = std::min(ry1, ry2);
    g.x2 = std::max(rx1, rx2);
    g.y2 = std::max(ry1, ry2);
    g.boxWidth = g.x2 - g.x1;
    g.boxHeight = g.y2 - g.y1;
    g.centerX = (g.x1 + g.x2) / 2.0;
    g.centerY = (g.y1 + g.y2) / 2.0;

    double angle = -startDeg * kDegToRad;
    g.sin1 = std::sin(angle);
    g.cos1 = std::cos(angle);
    angle -= extentDeg * kDegToRad;
    g.sin2 = std::sin(angle);
    g.cos2 = std::cos(angle);
    g.c1x = g.centerX + g.cos1 * g.boxWidth / 2.0;
    g.c1y = g.centerY + g.sin1 * g.boxHeight / 2.0;
    g.c2x = g.centerX + g.cos2 * g.boxWidth / 2.0;
    g.c2y = g.centerY + g.sin2 * g.boxHeight / 2.0;
    return g;
}

// Port of ComputeArcOutline's corner computation: the outer corner points at
// both ends of the arc (on the miter of the ellipse's two tangents).
void ArcCorners(const ArcGeometry& g, double width, Point& corner1, Point& corner2) {
    const double halfWidth = width / 2.0;
    double angle;
    if ((g.boxWidth * g.sin1 == 0.0) && (g.boxHeight * g.cos1 == 0.0)) {
        angle = 0.0;
    } else {
        angle = std::atan2(g.boxWidth * g.sin1, g.boxHeight * g.cos1);
    }
    corner1 = Point{g.c1x + std::cos(angle) * halfWidth,
                    g.c1y + std::sin(angle) * halfWidth};
    if ((g.boxWidth * g.sin2 == 0.0) && (g.boxHeight * g.cos2 == 0.0)) {
        angle = 0.0;
    } else {
        angle = std::atan2(g.boxWidth * g.sin2, g.boxHeight * g.cos2);
    }
    corner2 = Point{g.c2x + std::cos(angle) * halfWidth,
                    g.c2y + std::sin(angle) * halfWidth};
}

// The outline polygons of a pieslice: two "arm" polygons (6 and 7 points),
// assembled exactly like ComputeArcOutline's pieslice branch.
std::vector<Point> PiesliceArm1(const ArcGeometry& g, double width) {
    const Point vertex{g.centerX, g.centerY};
    double m1[2], m2[2];
    TkGetButtPoints(g.c1x, g.c1y, vertex.x, vertex.y, width, /*project=*/false, m1, m2);
    Point corner1, corner2;
    ArcCorners(g, width, corner1, corner2);
    return {
        {m1[0], m1[1]},
        {m2[0], m2[1]},
        {g.c1x + m2[0] - vertex.x, g.c1y + m2[1] - vertex.y},
        {corner1.x, corner1.y},
        {g.c1x + m1[0] - vertex.x, g.c1y + m1[1] - vertex.y},
        {m1[0], m1[1]},
    };
}

std::vector<Point> PiesliceArm2(const ArcGeometry& g, double width, double extentDeg,
                                const std::vector<Point>& arm1) {
    const Point vertex{g.centerX, g.centerY};
    double m1[2], m2[2];
    TkGetButtPoints(g.c2x, g.c2y, vertex.x, vertex.y, width, /*project=*/false, m1, m2);
    // The arm joins the first arm: the branch point is either arm1's first
    // or second butt point, chosen by the sweep direction/size.
    const Point branch = ((extentDeg > 180.0) || ((extentDeg < 0.0) && (extentDeg > -180.0)))
        ? arm1[0] : arm1[1];
    Point corner1, corner2;
    ArcCorners(g, width, corner1, corner2);
    return {
        {m1[0], m1[1]},
        {branch.x, branch.y},
        {m2[0], m2[1]},
        {g.c2x + m2[0] - vertex.x, g.c2y + m2[1] - vertex.y},
        {corner2.x, corner2.y},
        {g.c2x + m1[0] - vertex.x, g.c2y + m1[1] - vertex.y},
        {m1[0], m1[1]},
    };
}

// The 7-point outline polygon of a chord, assembled as in ComputeArcOutline.
std::vector<Point> ChordOutline(const ArcGeometry& g, double width) {
    double m1[2], m2[2];
    // p1 = c2, p2 = c1 (as in Tk: TkGetButtPoints(center2, center1, ...)).
    TkGetButtPoints(g.c2x, g.c2y, g.c1x, g.c1y, width, /*project=*/false, m1, m2);
    Point corner1, corner2;
    ArcCorners(g, width, corner1, corner2);
    return {
        {corner1.x, corner1.y},
        {m2[0], m2[1]},
        {g.c2x + m2[0] - g.c1x, g.c2y + m2[1] - g.c1y},
        {corner2.x, corner2.y},
        {g.c2x + m1[0] - g.c1x, g.c2y + m1[1] - g.c1y},
        {m1[0], m1[1]},
        {corner1.x, corner1.y},
    };
}

// Tk's angular range test (degrees), as used by the arc's endpoints/markers.
bool ArcAngleInRange(double baseAngleDeg, double startDeg, double extentDeg) {
    double tmp = baseAngleDeg - startDeg;
    if (tmp < 0.0) tmp += 360.0;
    return (tmp < extentDeg) || ((tmp - 360.0) > extentDeg);
}

}  // namespace

ArcItem::ArcItem(Id id, double x1, double y1, double x2, double y2, double startDeg, double extentDeg)
    : CanvasItem(id, "arc"), x1_(x1), y1_(y1), x2_(x2), y2_(y2) {
    if (x1_ > x2_) std::swap(x1_, x2_);
    if (y1_ > y2_) std::swap(y1_, y2_);
    // Tk defaults: black 1px outline, no fill.
    Style().outline = Color{0, 0, 0};
    SetAngles(startDeg, extentDeg);
}

void ArcItem::SetAngles(double startDeg, double extentDeg) {
    // Tk normalizes -start/-extent at configuration time (ConfigureArc):
    // start -= (int)(start/360) * 360, then if negative, += 360;
    // extent -= (int)(extent/360) * 360.
    startDeg -= static_cast<int>(startDeg / 360.0) * 360.0;
    if (startDeg < 0.0) startDeg += 360.0;
    extentDeg -= static_cast<int>(extentDeg / 360.0) * 360.0;
    start_ = startDeg;
    extent_ = extentDeg;
}

BBox ArcItem::GetBBox() const {
    if (IsHidden()) return BBox::Empty();
    // Tk: ComputeArcBbox — an integer box around the arc's extreme points,
    // padded by 1px (no outline GC) or (int)((width+1)/2 + 1) otherwise.
    const ArcGeometry g = ComputeArcGeometry(x1_, y1_, x2_, y2_, start_, extent_);

    int x1 = static_cast<int>(g.c1x), x2 = x1;   // (int) truncates toward zero
    int y1 = static_cast<int>(g.c1y), y2 = y1;
    auto include = [&](double px, double py) {
        const int ix = static_cast<int>(px + 0.5);
        if (ix < x1) x1 = ix;
        if (ix > x2) x2 = ix;
        const int iy = static_cast<int>(py + 0.5);
        if (iy < y1) y1 = iy;
        if (iy > y2) y2 = iy;
    };
    include(g.c2x, g.c2y);
    if (style_ == ArcStyle::PieSlice) {
        include(g.centerX, g.centerY);
    }
    if (ArcAngleInRange(0.0, start_, extent_)) {
        include(g.x2, g.centerY);          // 3 o'clock
    }
    if (ArcAngleInRange(90.0, start_, extent_)) {
        include(g.centerX, g.y1);          // 12 o'clock
    }
    if (ArcAngleInRange(180.0, start_, extent_)) {
        include(g.x1, g.centerY);          // 9 o'clock
    }
    if (ArcAngleInRange(270.0, start_, extent_)) {
        include(g.centerX, g.y2);          // 6 o'clock
    }

    double width = Style().width;
    if (width < 1.0) width = 1.0;
    double tmp = TkHasOutlineGC(Style()) ? ((width + 1.0) / 2.0 + 1) : 1.0;
    const int pad = static_cast<int>(tmp);
    return BBox{double(x1 - pad), double(y1 - pad), double(x2 + pad), double(y2 + pad)};
}

BBox ArcItem::DamageBBox() const {
    if (!DegeneratesToFullEllipse()) return GetBBox();
    const double width = std::max(x1_, x2_) - std::min(x1_, x2_);
    const double height = std::max(y1_, y2_) - std::min(y1_, y2_);
    double pad = Style().width * 0.5 + 2.0;
    return BBox{std::min(x1_, x2_) - pad, std::min(y1_, y2_) - pad,
                std::max(x1_, x2_) + pad, std::max(y1_, y2_) + pad};
}

int ArcItem::AreaOverlap(const double rect[4]) const {    // Tk's arc areaProc (ArcToArea in tkCanvArc.c).
    const ArcGeometry g = ComputeArcGeometry(x1_, y1_, x2_, y2_, start_, extent_);
    double width = Style().width;
    const bool hasGC = TkHasOutlineGC(Style());
    const bool fillPresent = Style().fill.has_value();
    const bool filled = fillPresent || !hasGC;
    if (!hasGC) width = 0.0;

    const double center[2] = {g.centerX, g.centerY};
    double tRect[4] = {rect[0] - center[0], rect[1] - center[1],
                       rect[2] - center[0], rect[3] - center[1]};
    const double rx = (g.x2 - g.centerX) + width / 2.0;
    const double ry = (g.y2 - g.centerY) + width / 2.0;

    // Extreme points of the arc: the two outer endpoints, the pie-slice
    // centre (pieslice && extent < 180 only), and the cardinal points that
    // lie on the swept arc.
    double points[20];
    double* pointPtr = points;
    double angle = -start_ * kDegToRad;
    pointPtr[0] = rx * std::cos(angle);
    pointPtr[1] = ry * std::sin(angle);
    angle -= extent_ * kDegToRad;
    pointPtr[2] = rx * std::cos(angle);
    pointPtr[3] = ry * std::sin(angle);
    int numPoints = 2;
    pointPtr += 4;

    if ((style_ == ArcStyle::PieSlice) && (extent_ < 180.0)) {
        pointPtr[0] = 0.0;
        pointPtr[1] = 0.0;
        ++numPoints;
        pointPtr += 2;
    }
    if (ArcAngleInRange(0.0, start_, extent_)) {
        pointPtr[0] = rx;
        pointPtr[1] = 0.0;
        ++numPoints;
        pointPtr += 2;
    }
    if (ArcAngleInRange(90.0, start_, extent_)) {
        pointPtr[0] = 0.0;
        pointPtr[1] = -ry;
        ++numPoints;
        pointPtr += 2;
    }
    if (ArcAngleInRange(180.0, start_, extent_)) {
        pointPtr[0] = -rx;
        pointPtr[1] = 0.0;
        ++numPoints;
        pointPtr += 2;
    }
    if (ArcAngleInRange(270.0, start_, extent_)) {
        pointPtr[0] = 0.0;
        pointPtr[1] = ry;
        ++numPoints;
    }

    int inside = (points[0] > tRect[0]) && (points[0] < tRect[2])
        && (points[1] > tRect[1]) && (points[1] < tRect[3]);
    for (std::size_t i = 1; i < static_cast<std::size_t>(numPoints); ++i) {
        const double* p = points + 2 * i;
        const int newInside = (p[0] > tRect[0]) && (p[0] < tRect[2])
            && (p[1] > tRect[1]) && (p[1] < tRect[3]);
        if (newInside != inside) return 0;
    }
    if (inside) return 1;

    // Test the non-curved boundaries (the arms of a pieslice / the chord).
    // These run regardless of fill, against the *untranslated* probe rect.
    if (style_ == ArcStyle::PieSlice) {
        if (width >= 1.0) {
            const std::vector<Point> arm1 = PiesliceArm1(g, width);
            if (TkPolygonToArea(arm1.data(), 6, rect) != -1) return 0;
            const std::vector<Point> arm2 = PiesliceArm2(g, width, extent_, arm1);
            if (TkPolygonToArea(arm2.data(), 7, rect) != -1) return 0;
        } else {
            const Point vertex{g.centerX, g.centerY};
            if ((TkLineToArea(g.centerX, g.centerY, g.c1x, g.c1y, rect) != -1)
                || (TkLineToArea(g.centerX, g.centerY, g.c2x, g.c2y, rect) != -1)) {
                return 0;
            }
        }
    } else if (style_ == ArcStyle::Chord) {
        if (width >= 1.0) {
            const std::vector<Point> chord = ChordOutline(g, width);
            if (TkPolygonToArea(chord.data(), 7, rect) != -1) return 0;
        } else {
            if (TkLineToArea(g.c1x, g.c1y, g.c2x, g.c2y, rect) != -1) return 0;
        }
    }

    // Check the arc's perimeter against the rect's four sides.
    if (TkHorizLineToArc(tRect[0], tRect[2], tRect[1], rx, ry, start_, extent_)
        || TkHorizLineToArc(tRect[0], tRect[2], tRect[3], rx, ry, start_, extent_)
        || TkVertLineToArc(tRect[0], tRect[1], tRect[3], rx, ry, start_, extent_)
        || TkVertLineToArc(tRect[2], tRect[1], tRect[3], rx, ry, start_, extent_)) {
        return 0;
    }
    if ((width > 1.0) && !filled) {
        const double irx = rx - width, iry = ry - width;
        if (TkHorizLineToArc(tRect[0], tRect[2], tRect[1], irx, iry, start_, extent_)
            || TkHorizLineToArc(tRect[0], tRect[2], tRect[3], irx, iry, start_, extent_)
            || TkVertLineToArc(tRect[0], tRect[1], tRect[3], irx, iry, start_, extent_)
            || TkVertLineToArc(tRect[2], tRect[1], tRect[3], irx, iry, start_, extent_)) {
            return 0;
        }
    }

    // Is the probe's lower-left corner inside the arc?
    if (ArcDistanceTo(Point{rect[0], rect[1]}) == 0.0) {
        return 0;
    }
    return -1;
}

void ArcItem::Translate(double dx, double dy) {
    x1_ += dx; x2_ += dx; y1_ += dy; y2_ += dy;
}

bool ArcItem::StyleFromString(const std::string& text, ArcStyle& out) {
    const std::string name = ToLower(text);
    if (name == "pieslice" || name == "pie") { out = ArcStyle::PieSlice; return true; }
    if (name == "chord") { out = ArcStyle::Chord; return true; }
    if (name == "arc") { out = ArcStyle::Arc; return true; }
    return false;
}

const char* ArcItem::StyleName(ArcStyle style) {
    switch (style) {
        case ArcStyle::PieSlice: return "pieslice";
        case ArcStyle::Chord: return "chord";
        case ArcStyle::Arc: return "arc";
    }
    return "pieslice";
}

bool ArcItem::SetCoordinates(const std::vector<double>& coords) {
    if (coords.size() != 4) return false;
    x1_ = coords[0]; y1_ = coords[1]; x2_ = coords[2]; y2_ = coords[3];
    if (x1_ > x2_) std::swap(x1_, x2_);
    if (y1_ > y2_) std::swap(y1_, y2_);
    return true;
}

std::vector<Point> ArcItem::SampleArc() const {
    std::vector<Point> points;
    SampleArc(points);
    return points;
}

void ArcItem::SampleArc(std::vector<Point>& out) const {
    const double left = std::min(x1_, x2_), right = std::max(x1_, x2_);
    const double top = std::min(y1_, y2_), bottom = std::max(y1_, y2_);
    const double cx = (left + right) * 0.5, cy = (top + bottom) * 0.5;
    const double rx = (right - left) * 0.5, ry = (bottom - top) * 0.5;

    const double start = start_ * kDegToRad;
    const double sweep = extent_ * kDegToRad;
    const int segments = std::clamp(
        static_cast<int>(std::ceil(std::fabs(sweep) / (kDegToRad * 2.0))), 8, 360);

    out.reserve(static_cast<std::size_t>(segments) + 1);
    for (int i = 0; i <= segments; ++i) {
        const double angle = start + sweep * static_cast<double>(i) / segments;
        // Canvas coords are y-down; Tk angles grow counter-clockwise.
        out.push_back(Point{cx + rx * std::cos(angle), cy - ry * std::sin(angle)});
    }
}

bool ArcItem::DegeneratesToFullEllipse() const {
    // Tk on Windows draws the arc with GDI's Arc(), which rounds the arc's
    // two end points to whole pixels.  When both points end up on the same
    // pixel, GDI draws the *complete* ellipse instead of the (sub-pixel)
    // arc.  The rule is reproduced here so that such an arc looks exactly
    // like it does on the native canvas -- TinUI's wait bar draws its ring
    // with "extent 361", which relies on it.
    const double width = x2_ - x1_;
    const double height = y2_ - y1_;
    const double cx = (x1_ + x2_) * 0.5;
    const double cy = (y1_ + y2_) * 0.5;

    const double a1 = start_ * kDegToRad;
    const double a2 = (start_ + extent_) * kDegToRad;
    const int xStart = static_cast<int>(cx + std::cos(a1) * width * 0.5 + 0.5);
    const int yStart = static_cast<int>(cy - std::sin(a1) * height * 0.5 + 0.5);
    const int xEnd = static_cast<int>(cx + std::cos(a2) * width * 0.5 + 0.5);
    const int yEnd = static_cast<int>(cy - std::sin(a2) * height * 0.5 + 0.5);
    return xStart == xEnd && yStart == yEnd;
}

double ArcItem::ArcDistanceTo(Point p) const {
    const ArcGeometry g = ComputeArcGeometry(x1_, y1_, x2_, y2_, start_, extent_);
    double width = Style().width;
    const bool hasGC = TkHasOutlineGC(Style());
    const bool fillPresent = Style().fill.has_value();
    const bool filled = fillPresent || !hasGC;
    if (!hasGC) width = 0.0;

    // Angle of the point relative to the arc's centre.
    const double t1 = (g.boxHeight != 0.0) ? (p.y - g.centerY) / g.boxHeight : 0.0;
    const double t2 = (g.boxWidth != 0.0) ? (p.x - g.centerX) / g.boxWidth : 0.0;
    double pointAngle;
    if ((t1 == 0.0) && (t2 == 0.0)) {
        pointAngle = 0.0;
    } else {
        pointAngle = -std::atan2(t1, t2) / kDegToRad;
    }
    double diff = pointAngle - start_;
    diff -= static_cast<int>(diff / 360.0) * 360.0;
    if (diff < 0.0) diff += 360.0;
    const bool angleInRange = (diff <= extent_) || ((extent_ < 0.0) && ((diff - 360.0) >= extent_));

    if (style_ == ArcStyle::Arc) {
        if (angleInRange) {
            const double bbox[4] = {g.x1, g.y1, g.x2, g.y2};
            return TkOvalToPoint(bbox, width, /*filled=*/false, p);
        }
        const double dist1 = std::hypot(p.x - g.c1x, p.y - g.c1y);
        const double dist2 = std::hypot(p.x - g.c2x, p.y - g.c2y);
        return std::min(dist1, dist2);
    }

    if (style_ == ArcStyle::PieSlice) {
        double dist;
        if (width > 1.0) {
            const std::vector<Point> arm1 = PiesliceArm1(g, width);
            const std::vector<Point> arm2 = PiesliceArm2(g, width, extent_, arm1);
            dist = TkPolygonToPoint(arm1.data(), 6, p.x, p.y);
            const double d2 = TkPolygonToPoint(arm2.data(), 7, p.x, p.y);
            if (d2 < dist) dist = d2;
        } else {
            const Point vertex{g.centerX, g.centerY};
            dist = HitTest::DistancePointToSegment(p, vertex, Point{g.c1x, g.c1y});
            const double d2 = HitTest::DistancePointToSegment(p, vertex, Point{g.c2x, g.c2y});
            if (d2 < dist) dist = d2;
        }
        if (angleInRange) {
            const double bbox[4] = {g.x1, g.y1, g.x2, g.y2};
            const double d2 = TkOvalToPoint(bbox, width, filled, p);
            if (d2 < dist) dist = d2;
        }
        return dist;
    }

    // Chord style.
    double dist;
    if (width > 1.0) {
        const std::vector<Point> chord = ChordOutline(g, width);
        dist = TkPolygonToPoint(chord.data(), 7, p.x, p.y);
    } else {
        dist = HitTest::DistancePointToSegment(p, Point{g.c1x, g.c1y}, Point{g.c2x, g.c2y});
    }
    const Point vertexPoly[4] = {
        {g.centerX, g.centerY},
        {g.c1x, g.c1y},
        {g.c2x, g.c2y},
        {g.centerX, g.centerY},
    };
    const double polyDist = TkPolygonToPoint(vertexPoly, 4, p.x, p.y);
    if (angleInRange) {
        if ((extent_ < -180.0) || (extent_ > 180.0) || (polyDist > 0.0)) {
            const double bbox[4] = {g.x1, g.y1, g.x2, g.y2};
            const double d2 = TkOvalToPoint(bbox, width, filled, p);
            if (d2 < dist) dist = d2;
        }
    } else {
        if ((extent_ < -180.0) || (extent_ > 180.0)) {
            if (filled && (polyDist < dist)) dist = polyDist;
        }
    }
    return dist;
}

double ArcItem::DistanceTo(Point p) const {
    return ArcDistanceTo(p);
}

void ArcItem::Draw(Renderer& renderer, Path& path) const {
    if (IsHidden()) return;

    const double left = std::min(x1_, x2_), right = std::max(x1_, x2_);
    const double top = std::min(y1_, y2_), bottom = std::max(y1_, y2_);
    if (left == right && top == bottom) return;
    if (extent_ == 0.0) return;  // Tk draws nothing for a zero-extent arc

    // The arc points go straight into the caller's path buffer: a repaint
    // must not allocate a point list per arc.
    path.points.clear();
    SampleArc(path.points);
    if (path.points.size() < 2) return;
    if (DegeneratesToFullEllipse()) {
        const double cx = (left + right) * 0.5, cy = (top + bottom) * 0.5;
        const double rx = (right - left) * 0.5, ry = (bottom - top) * 0.5;
        const int segments = 180;
        path.points.clear();
        path.points.reserve(static_cast<std::size_t>(segments) + 1);
        for (int i = 0; i <= segments; ++i) {
            const double angle = 2.0 * 3.14159265358979323846 *
                                 static_cast<double>(i) / segments;
            path.points.push_back(Point{cx + rx * std::cos(angle), cy - ry * std::sin(angle)});
        }
    }

    const auto fill = ItemStateFill(Style());
    const auto& outline = Style().outline;

    switch (style_) {
        case ArcStyle::Arc: {
            // Only the arc line; -fill is meaningless here.
            if (!outline) return;
            path.closed = false;
            renderer.drawLine(path, ItemPenStyle(*outline, Style(),
                                                  CapStyle::Flat, JoinStyle::Bevel));
            break;
        }
        case ArcStyle::Chord: {
            path.closed = true;  // closes back to the arc's start point: the chord
            if (fill) renderer.fillPath(path, BrushStyle{*fill});
            if (outline) renderer.drawLine(path, ItemPenStyle(*outline, Style()));
            break;
        }
        case ArcStyle::PieSlice: {
            const double cx = (left + right) * 0.5, cy = (top + bottom) * 0.5;
            path.closed = true;
            path.points.insert(path.points.begin(), Point{cx, cy});
            if (fill) renderer.fillPath(path, BrushStyle{*fill});
            if (outline) renderer.drawLine(path, ItemPenStyle(*outline, Style()));
            break;
        }
    }
}
