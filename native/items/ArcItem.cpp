#include "ArcItem.h"

#include <algorithm>

ArcItem::ArcItem(Id id, double x1, double y1, double x2, double y2, double startDeg, double extentDeg)
    : CanvasItem(id, "arc"), x1_(x1), y1_(y1), x2_(x2), y2_(y2), start_(startDeg), extent_(extentDeg) {}

BBox ArcItem::GetBBox() const {
    return BBox{std::min(x1_, x2_), std::min(y1_, y2_),
                std::max(x1_, x2_), std::max(y1_, y2_)};
}

void ArcItem::Translate(double dx, double dy) {
    x1_ += dx; x2_ += dx; y1_ += dy; y2_ += dy;
}

void ArcItem::Draw(Renderer&) const {
    // TODO(M9): Direct2D arc/chord/pie segment painting.
}
