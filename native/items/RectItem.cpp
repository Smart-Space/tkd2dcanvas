#include "RectItem.h"

#include <algorithm>
#include <utility>

RectItem::RectItem(Id id, double x1, double y1, double x2, double y2)
    : CanvasItem(id, "rectangle"), x1_(x1), y1_(y1), x2_(x2), y2_(y2) {}

BBox RectItem::GetBBox() const {
    return BBox{std::min(x1_, x2_), std::min(y1_, y2_),
                std::max(x1_, x2_), std::max(y1_, y2_)};
}

void RectItem::Translate(double dx, double dy) {
    x1_ += dx; x2_ += dx; y1_ += dy; y2_ += dy;
}

void RectItem::SetCoordinates(double x1, double y1, double x2, double y2) {
    x1_ = x1; y1_ = y1; x2_ = x2; y2_ = y2;
}

void RectItem::Draw(Renderer&) const {
    // TODO(M4): emit a Path and call renderer.drawPath(path, pen, brush).
}
