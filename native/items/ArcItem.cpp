#include "ArcItem.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <string>

#include "tkd2dcanvas/HitTest.h"

namespace {
constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
constexpr double kTwoPi = 6.28318530717958647692;

std::string ToLower(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (unsigned char c : text) out.push_back(static_cast<char>(std::tolower(c)));
    return out;
}
}  // namespace

ArcItem::ArcItem(Id id, double x1, double y1, double x2, double y2, double startDeg, double extentDeg)
    : CanvasItem(id, "arc"), x1_(x1), y1_(y1), x2_(x2), y2_(y2),
      start_(startDeg), extent_(extentDeg) {
    // Tk defaults: black 1px outline, no fill.
    Style().outline = Color{0, 0, 0};
}

BBox ArcItem::GetBBox() const {
    return BBox{std::min(x1_, x2_), std::min(y1_, y2_),
                std::max(x1_, x2_), std::max(y1_, y2_)};
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
    return true;
}

std::vector<Point> ArcItem::SampleArc() const {
    const double left = std::min(x1_, x2_), right = std::max(x1_, x2_);
    const double top = std::min(y1_, y2_), bottom = std::max(y1_, y2_);
    const double cx = (left + right) * 0.5, cy = (top + bottom) * 0.5;
    const double rx = (right - left) * 0.5, ry = (bottom - top) * 0.5;

    const double start = start_ * kDegToRad;
    const double sweep = extent_ * kDegToRad;
    const int segments = std::clamp(
        static_cast<int>(std::ceil(std::fabs(sweep) / (kDegToRad * 2.0))), 8, 360);

    std::vector<Point> points;
    points.reserve(static_cast<std::size_t>(segments) + 1);
    for (int i = 0; i <= segments; ++i) {
        const double angle = start + sweep * static_cast<double>(i) / segments;
        // Canvas coords are y-down; Tk angles grow counter-clockwise.
        points.push_back(Point{cx + rx * std::cos(angle), cy - ry * std::sin(angle)});
    }
    return points;
}

bool ArcItem::HitTest(Point p, double tolerance) const {
    const std::vector<Point> arc = SampleArc();
    if (arc.size() < 2) return false;

    const auto nearArc = [&]() {
        for (std::size_t i = 0; i + 1 < arc.size(); ++i) {
            if (HitTest::DistancePointToSegment(p, arc[i], arc[i + 1]) <= tolerance) return true;
        }
        return false;
    };

    switch (style_) {
        case ArcStyle::Arc:
            return nearArc();
        case ArcStyle::Chord: {
            if (HitTest::PointInPolygon(p, arc)) return true;
            if (HitTest::DistancePointToSegment(p, arc.back(), arc.front()) <= tolerance) return true;
            return nearArc();
        }
        case ArcStyle::PieSlice: {
            const BBox box = GetBBox();
            std::vector<Point> wedge;
            wedge.reserve(arc.size() + 1);
            wedge.push_back(Point{box.CenterX(), box.CenterY()});
            wedge.insert(wedge.end(), arc.begin(), arc.end());
            if (HitTest::PointInPolygon(p, wedge)) return true;
            if (HitTest::DistancePointToSegment(p, wedge.front(), arc.front()) <= tolerance) return true;
            if (HitTest::DistancePointToSegment(p, wedge.front(), arc.back()) <= tolerance) return true;
            return nearArc();
        }
    }
    return false;
}

void ArcItem::Draw(Renderer& renderer) const {
    const double left = std::min(x1_, x2_), right = std::max(x1_, x2_);
    const double top = std::min(y1_, y2_), bottom = std::max(y1_, y2_);
    if (left == right && top == bottom) return;

    std::vector<Point> arc = SampleArc();
    if (arc.size() < 2) return;

    const auto& fill = Style().fill;
    const auto& outline = Style().outline;

    switch (style_) {
        case ArcStyle::Arc: {
            // Only the arc line; -fill is meaningless here.
            if (!outline) return;
            Path path;
            path.closed = false;
            path.points = std::move(arc);
            renderer.drawLine(path, ItemPenStyle(*outline, Style(),
                                                 CapStyle::Flat, JoinStyle::Bevel));
            break;
        }
        case ArcStyle::Chord: {
            Path path;
            path.closed = true;  // closes back to the arc's start point: the chord
            path.points = std::move(arc);
            if (fill) renderer.fillPath(path, BrushStyle{*fill});
            if (outline) renderer.drawLine(path, ItemPenStyle(*outline, Style()));
            break;
        }
        case ArcStyle::PieSlice: {
            const double cx = (left + right) * 0.5, cy = (top + bottom) * 0.5;
            Path path;
            path.closed = true;
            path.points.reserve(arc.size() + 1);
            path.points.push_back(Point{cx, cy});
            path.points.insert(path.points.end(), arc.begin(), arc.end());
            if (fill) renderer.fillPath(path, BrushStyle{*fill});
            if (outline) renderer.drawLine(path, ItemPenStyle(*outline, Style()));
            break;
        }
    }
}
