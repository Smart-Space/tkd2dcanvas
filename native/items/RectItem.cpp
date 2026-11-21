#include "RectItem.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "tkd2dcanvas/TkAreaOverlap.h"

RectItem::RectItem(Id id, double x1, double y1, double x2, double y2)
    : CanvasItem(id, "rectangle"), x1_(x1), y1_(y1), x2_(x2), y2_(y2) {
    if (x1_ > x2_) std::swap(x1_, x2_);
    if (y1_ > y2_) std::swap(y1_, y2_);
    // Tk defaults: no fill, black 1px outline.
    Style().outline = Color{0, 0, 0};
}

BBox RectItem::GetBBox() const {
    if (IsHidden()) return BBox::Empty();
    // Tk: the bbox is an integer box with a pen-width bloat (ComputeRectOvalBBox).
    return TkRectOvalBBox(x1_, y1_, x2_, y2_, TkHasOutlineGC(Style()), Style().width);
}

int RectItem::AreaOverlap(const double rect[4]) const {
    // Tk's rect areaProc (RectToArea in tkRectOval.c), on the raw bbox.
    const double x1 = std::min(x1_, x2_), x2 = std::max(x1_, x2_);
    const double y1 = std::min(y1_, y2_), y2 = std::max(y1_, y2_);
    const double halfWidth = TkHasOutlineGC(Style()) ? Style().width / 2.0 : 0.0;

    if ((rect[2] <= x1 - halfWidth) || (rect[0] >= x2 + halfWidth)
        || (rect[3] <= y1 - halfWidth) || (rect[1] >= y2 + halfWidth)) {
        return -1;
    }
    if (!Style().fill.has_value() && TkHasOutlineGC(Style())
        && (rect[0] >= x1 + halfWidth) && (rect[1] >= y1 + halfWidth)
        && (rect[2] <= x2 - halfWidth) && (rect[3] <= y2 - halfWidth)) {
        return -1;
    }
    if ((rect[0] <= x1 - halfWidth) && (rect[1] <= y1 - halfWidth)
        && (rect[2] >= x2 + halfWidth) && (rect[3] >= y2 + halfWidth)) {
        return 1;
    }
    return 0;
}

void RectItem::Translate(double dx, double dy) {
    x1_ += dx; x2_ += dx; y1_ += dy; y2_ += dy;
}

double RectItem::DistanceTo(Point p) const {
    const double left = std::min(x1_, x2_), right = std::max(x1_, x2_);
    const double top = std::min(y1_, y2_), bottom = std::max(y1_, y2_);
    const BBox box{left, top, right, bottom};
    if (!box.IsValid()) return 0.0;

    const double dx = (p.x < box.left) ? box.left - p.x
                     : (p.x > box.right) ? p.x - box.right
                                         : 0.0;
    const double dy = (p.y < box.top) ? box.top - p.y
                     : (p.y > box.bottom) ? p.y - box.bottom
                                          : 0.0;

    // Outside the rectangle: the distance to the rectangle.
    if (dx > 0.0 || dy > 0.0) {
        return std::sqrt(dx * dx + dy * dy);
    }

    // Inside: a filled rectangle is hit everywhere, an unfilled one only
    // near its outline (Tk behaves the same way).
    if (Style().fill) return 0.0;
    return std::min(std::min(p.x - box.left, box.right - p.x),
                    std::min(p.y - box.top, box.bottom - p.y));
}

bool RectItem::SetCoordinates(const std::vector<double>& coords) {
    if (coords.size() != 4) return false;
    x1_ = coords[0]; y1_ = coords[1]; x2_ = coords[2]; y2_ = coords[3];
    if (x1_ > x2_) std::swap(x1_, x2_);
    if (y1_ > y2_) std::swap(y1_, y2_);
    return true;
}

void RectItem::SetCoordinates(double x1, double y1, double x2, double y2) {
    x1_ = x1; y1_ = y1; x2_ = x2; y2_ = y2;
}

void RectItem::Draw(Renderer& renderer, Path& path) const {
    if (IsHidden()) return;

    const double left = std::min(x1_, x2_), right = std::max(x1_, x2_);
    const double top = std::min(y1_, y2_), bottom = std::max(y1_, y2_);

    path.closed = true;
    path.points.clear();
    path.points.push_back(Point{left, top});
    path.points.push_back(Point{right, top});
    path.points.push_back(Point{right, bottom});
    path.points.push_back(Point{left, bottom});

    if (const auto& fill = ItemStateFill(Style())) {
        renderer.fillPath(path, BrushStyle{*fill});
    }
    if (const auto& outline = Style().outline) {
        renderer.drawLine(path, ItemPenStyle(*outline, Style()));
    }
}
