#include "RectItem.h"

#include <algorithm>
#include <utility>

RectItem::RectItem(Id id, double x1, double y1, double x2, double y2)
    : CanvasItem(id, "rectangle"), x1_(x1), y1_(y1), x2_(x2), y2_(y2) {
    // Tk defaults: no fill, black 1px outline.
    Style().outline = Color{0, 0, 0};
}

BBox RectItem::GetBBox() const {
    return BBox{std::min(x1_, x2_), std::min(y1_, y2_),
                std::max(x1_, x2_), std::max(y1_, y2_)};
}

void RectItem::Translate(double dx, double dy) {
    x1_ += dx; x2_ += dx; y1_ += dy; y2_ += dy;
}

bool RectItem::SetCoordinates(const std::vector<double>& coords) {
    if (coords.size() != 4) return false;
    x1_ = coords[0]; y1_ = coords[1]; x2_ = coords[2]; y2_ = coords[3];
    return true;
}

void RectItem::SetCoordinates(double x1, double y1, double x2, double y2) {
    x1_ = x1; y1_ = y1; x2_ = x2; y2_ = y2;
}

void RectItem::Draw(Renderer& renderer) const {
    const double left = std::min(x1_, x2_), right = std::max(x1_, x2_);
    const double top = std::min(y1_, y2_), bottom = std::max(y1_, y2_);

    Path path;
    path.closed = true;
    path.points = {{left, top}, {right, top}, {right, bottom}, {left, bottom}};

    if (const auto& fill = Style().fill) {
        renderer.fillPath(path, BrushStyle{*fill});
    }
    if (const auto& outline = Style().outline) {
        renderer.drawLine(path, ItemPenStyle(*outline, Style()));
    }
}
