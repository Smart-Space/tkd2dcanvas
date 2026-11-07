#include "OvalItem.h"

#include <algorithm>

#include "tkd2dcanvas/HitTest.h"

OvalItem::OvalItem(Id id, double x1, double y1, double x2, double y2)
    : CanvasItem(id, "oval"), x1_(x1), y1_(y1), x2_(x2), y2_(y2) {}

BBox OvalItem::GetBBox() const {
    return BBox{std::min(x1_, x2_), std::min(y1_, y2_),
                std::max(x1_, x2_), std::max(y1_, y2_)};
}

void OvalItem::Translate(double dx, double dy) {
    x1_ += dx; x2_ += dx; y1_ += dy; y2_ += dy;
}

bool OvalItem::HitTest(Point p, double /*tolerance*/) const {
    const BBox box = GetBBox();
    return HitTest::PointInEllipse(p, box.CenterX(), box.CenterY(),
                                   box.Width() * 0.5, box.Height() * 0.5);
}

void OvalItem::SetCoordinates(double x1, double y1, double x2, double y2) {
    x1_ = x1; y1_ = y1; x2_ = x2; y2_ = y2;
}

void OvalItem::Draw(Renderer&) const {
    // TODO(M4): path with ellipse arcs.
}
