#include "ImageItem.h"

#include <utility>

ImageItem::ImageItem(Id id, double x, double y, std::string imageName)
    : CanvasItem(id, "image"), x_(x), y_(y), imageName_(std::move(imageName)) {}

BBox ImageItem::GetBBox() const {
    if (width_ <= 0 || height_ <= 0) {
        return BBox{x_, y_, x_ + 1.0, y_ + 1.0};  // size unknown until resolved
    }
    return BBox{x_, y_, x_ + width_, y_ + height_};
}

void ImageItem::Translate(double dx, double dy) {
    x_ += dx;
    y_ += dy;
}

void ImageItem::Draw(Renderer&) const {
    // TODO(M9): resolve ImageRef via CanvasTkImage and call renderer.drawImage().
}
