#include "BitmapItem.h"

BitmapItem::BitmapItem(Id id, double x, double y, std::string bitmapName)
    : CanvasItem(id, "bitmap"), x_(x), y_(y), bitmapName_(std::move(bitmapName)) {}

BBox BitmapItem::GetBBox() const {
    if (width_ <= 0 || height_ <= 0) {
        return BBox{x_, y_, x_ + 16.0, y_ + 16.0};  // nominal XBM size
    }
    return BBox{x_, y_, x_ + width_, y_ + height_};
}

void BitmapItem::Translate(double dx, double dy) {
    x_ += dx;
    y_ += dy;
}

void BitmapItem::Draw(Renderer&) const {
    // TODO(M9): resolve bitmap resource and rasterize with Direct2D.
}
