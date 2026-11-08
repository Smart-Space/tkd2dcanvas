#include "ImageItem.h"

#include <algorithm>
#include <cstdint>
#include <utility>

#include "tkd2dcanvas/Transform2D.h"

ImageItem::ImageItem(Id id, double x, double y, std::string imageName)
    : CanvasItem(id, "image"), x_(x), y_(y), imageName_(std::move(imageName)) {}

BBox ImageItem::GetBBox() const {
    const double x = TkAnchorRound(x_);
    const double y = TkAnchorRound(y_);
    if (width_ <= 0 || height_ <= 0) {
        return BBox{x, y, x, y};
    }
    const double w = static_cast<double>(width_);
    const double h = static_cast<double>(height_);
    const Point topLeft = AnchorTopLeft(anchor_, x, y, w, h);
    return BBox{topLeft.x, topLeft.y, topLeft.x + w, topLeft.y + h};
}

void ImageItem::Translate(double dx, double dy) {
    x_ += dx;
    y_ += dy;
}

std::vector<double> ImageItem::GetCoordinates() const { return {x_, y_}; }

bool ImageItem::SetCoordinates(const std::vector<double>& coords) {
    if (coords.size() != 2) return false;
    x_ = coords[0];
    y_ = coords[1];
    return true;
}

void ImageItem::SetPixels(std::vector<std::uint8_t> rgba, int width, int height) {
    if (width <= 0 || height <= 0 || rgba.size() != static_cast<std::size_t>(width) * height * 4) {
        ClearPixels();
        return;
    }
    pixels_ = std::move(rgba);
    width_ = width;
    height_ = height;
    ++generation_;
}

void ImageItem::ClearPixels() {
    pixels_.clear();
    width_ = 0;
    height_ = 0;
    ++generation_;
}

void ImageItem::Draw(Renderer& renderer) const {
    if (pixels_.empty() || width_ <= 0 || height_ <= 0) return;

    const double w = static_cast<double>(width_);
    const double h = static_cast<double>(height_);
    // Draw at the same rounded position the bbox uses (Tk draws at header.x1).
    const Point topLeft = AnchorTopLeft(anchor_, TkAnchorRound(x_), TkAnchorRound(y_), w, h);

    ImageRef ref;
    ref.pixels = pixels_.data();
    ref.width = width_;
    ref.height = height_;
    ref.stride = width_ * 4;
    ref.cacheKey = reinterpret_cast<const void*>(static_cast<std::uintptr_t>(GetId()));
    ref.generation = generation_;
    renderer.drawImage(ref, Transform2D::Translate(topLeft.x, topLeft.y));
}
