#include "WindowItem.h"

WindowItem::WindowItem(Id id, double x, double y, std::string windowPath)
    : CanvasItem(id, "window"), x_(x), y_(y), windowPath_(std::move(windowPath)) {}

BBox WindowItem::GetBBox() const {
    const double w = width_ > 0 ? static_cast<double>(width_) : 1.0;
    const double h = height_ > 0 ? static_cast<double>(height_) : 1.0;
    return BBox{x_, y_, x_ + w, y_ + h};
}

void WindowItem::Translate(double dx, double dy) {
    x_ += dx;
    y_ += dy;
}

void WindowItem::Draw(Renderer&) const {
    // No-op by design: the child widget paints itself. The canvas only
    // needs the geometry for scrolling/ clipping/ layering (M10).
}
