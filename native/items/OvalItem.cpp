#include "OvalItem.h"

#include <algorithm>
#include <cmath>

#include "tkd2dcanvas/HitTest.h"

OvalItem::OvalItem(Id id, double x1, double y1, double x2, double y2)
    : CanvasItem(id, "oval"), x1_(x1), y1_(y1), x2_(x2), y2_(y2) {
    // Tk defaults: no fill, black 1px outline.
    Style().outline = Color{0, 0, 0};
}

BBox OvalItem::GetBBox() const {
    return BBox{std::min(x1_, x2_), std::min(y1_, y2_),
                std::max(x1_, x2_), std::max(y1_, y2_)};
}

void OvalItem::Translate(double dx, double dy) {
    x1_ += dx; x2_ += dx; y1_ += dy; y2_ += dy;
}

double OvalItem::DistanceTo(Point p) const {
    const BBox box = GetBBox();
    const double rx = box.Width() * 0.5;
    const double ry = box.Height() * 0.5;
    if (rx <= 0.0 || ry <= 0.0) {
        // Degenerate ellipse: fall back to the bounding box.
        return CanvasItem::DistanceTo(p);
    }

    const double cx = box.CenterX();
    const double cy = box.CenterY();

    // Work in the unit-circle space of the ellipse: the direction from the
    // centre is preserved, the radius is scaled. This mirrors Tk's
    // TkOvalToPoint (an approximation for eccentric ovals).
    const double nx = (p.x - cx) / rx;
    const double ny = (p.y - cy) / ry;
    const double scaled = std::sqrt(nx * nx + ny * ny);
    const double dx = p.x - cx;
    const double dy = p.y - cy;
    const double distToCenter = std::sqrt(dx * dx + dy * dy);

    if (scaled > 1.0) {
        // Outside: the distance to the ellipse.
        return (distToCenter / scaled) * (scaled - 1.0);
    }

    // Inside: a filled oval is hit everywhere, an unfilled one only near its
    // outline (Tk behaves the same way).
    if (Style().fill) return 0.0;
    if (scaled < 1e-10) return std::min(rx, ry);  // the centre
    return (distToCenter / scaled) * (1.0 - scaled);
}

bool OvalItem::SetCoordinates(const std::vector<double>& coords) {
    if (coords.size() != 4) return false;
    x1_ = coords[0]; y1_ = coords[1]; x2_ = coords[2]; y2_ = coords[3];
    return true;
}

void OvalItem::SetCoordinates(double x1, double y1, double x2, double y2) {
    x1_ = x1; y1_ = y1; x2_ = x2; y2_ = y2;
}

void OvalItem::Draw(Renderer& renderer) const {
    const double left = std::min(x1_, x2_), right = std::max(x1_, x2_);
    const double top = std::min(y1_, y2_), bottom = std::max(y1_, y2_);
    const double cx = (left + right) * 0.5, cy = (top + bottom) * 0.5;
    const double rx = (right - left) * 0.5, ry = (bottom - top) * 0.5;
    if (rx <= 0.0 && ry <= 0.0) return;

    // Renderer paths are polylines; approximate the ellipse with enough
    // segments to look smooth at any zoom we support today.
    constexpr int kSegments = 64;
    constexpr double kTwoPi = 6.28318530717958647692;

    Path path;
    path.closed = true;
    path.points.reserve(kSegments);
    for (int i = 0; i < kSegments; ++i) {
        const double angle = kTwoPi * static_cast<double>(i) / kSegments;
        path.points.push_back(Point{cx + rx * std::cos(angle), cy + ry * std::sin(angle)});
    }

    if (const auto& fill = Style().fill) {
        renderer.fillPath(path, BrushStyle{*fill});
    }
    if (const auto& outline = Style().outline) {
        renderer.drawLine(path, ItemPenStyle(*outline, Style()));
    }
}
