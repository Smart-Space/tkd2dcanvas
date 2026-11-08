#include "WindowItem.h"

#include <algorithm>
#include <cmath>

#include "tkd2dcanvas/Anchor.h"

namespace {

double RoundHalfAway(double v) {
    return v >= 0.0 ? std::floor(v + 0.5) : std::ceil(v - 0.5);
}

}  // namespace

WindowItem::WindowItem(Id id, double x, double y, std::string windowPath)
    : CanvasItem(id, "window"), x_(x), y_(y), windowPath_(std::move(windowPath)) {}

BBox WindowItem::GetBBox() const {
    const double x = RoundHalfAway(x_);
    const double y = RoundHalfAway(y_);
    if (IsHidden() || windowPath_.empty()) {
        return BBox{x, y, x + 1.0, y + 1.0};
    }
    const double w = static_cast<double>(EffectiveWidth());
    const double h = static_cast<double>(EffectiveHeight());
    const Point topLeft = AnchorTopLeft(anchor_, x, y, w, h);
    return BBox{topLeft.x, topLeft.y, topLeft.x + w, topLeft.y + h};
}

void WindowItem::Translate(double dx, double dy) {
    x_ += dx;
    y_ += dy;
}

void WindowItem::Scale(double xOrigin, double yOrigin, double xScale, double yScale) {
    x_ = xOrigin + (x_ - xOrigin) * xScale;
    y_ = yOrigin + (y_ - yOrigin) * yScale;
    if (itemWidth_ > 0) itemWidth_ = std::max(0, static_cast<int>(xScale * itemWidth_));
    if (itemHeight_ > 0) itemHeight_ = std::max(0, static_cast<int>(yScale * itemHeight_));
}

void WindowItem::Draw(Renderer&) const {}

std::vector<double> WindowItem::GetCoordinates() const { return {x_, y_}; }

bool WindowItem::SetCoordinates(const std::vector<double>& coords) {
    if (coords.size() != 2) return false;
    x_ = coords[0];
    y_ = coords[1];
    return true;
}

void WindowItem::SetNaturalSize(int width, int height) {
    naturalWidth_ = std::max(1, width);
    naturalHeight_ = std::max(1, height);
}

void WindowItem::SetRequestedSize(int width, int height) {
    itemWidth_ = std::max(0, width);
    itemHeight_ = std::max(0, height);
}

int WindowItem::EffectiveWidth() const {
    return std::max(1, itemWidth_ > 0 ? itemWidth_ : naturalWidth_);
}

int WindowItem::EffectiveHeight() const {
    return std::max(1, itemHeight_ > 0 ? itemHeight_ : naturalHeight_);
}
