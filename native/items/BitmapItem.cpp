#include "BitmapItem.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

#include "tkd2dcanvas/Transform2D.h"

BitmapItem::BitmapItem(Id id, double x, double y, std::string bitmapName)
    : CanvasItem(id, "bitmap"), x_(x), y_(y), bitmapName_(std::move(bitmapName)) {}

BBox BitmapItem::GetBBox() const {
    if (IsHidden()) return BBox::Empty();
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

void BitmapItem::Translate(double dx, double dy) {
    x_ += dx;
    y_ += dy;
}

std::vector<double> BitmapItem::GetCoordinates() const { return {x_, y_}; }

bool BitmapItem::SetCoordinates(const std::vector<double>& coords) {
    if (coords.size() != 2) return false;
    x_ = coords[0];
    y_ = coords[1];
    return true;
}

void BitmapItem::SetBitmapBits(std::vector<std::uint8_t> bits, int width, int height) {
    const std::size_t expected =
        width > 0 && height > 0
            ? static_cast<std::size_t>((width + 7) / 8) * static_cast<std::size_t>(height)
            : 0;
    if (width <= 0 || height <= 0 || bits.size() != expected) {
        bits_.clear();
        width_ = 0;
        height_ = 0;
        pixels_.clear();
        ++generation_;
        return;
    }
    bits_ = std::move(bits);
    width_ = width;
    height_ = height;
    BakePixels();
}

void BitmapItem::SetForeground(std::optional<Color> color) {
    foreground_ = std::move(color);
    BakePixels();
}

void BitmapItem::SetBackground(std::optional<Color> color) {
    background_ = std::move(color);
    BakePixels();
}

void BitmapItem::BakePixels() {
    ++generation_;
    if (bits_.empty() || width_ <= 0 || height_ <= 0) {
        pixels_.clear();
        return;
    }
    const std::size_t rowBytes = static_cast<std::size_t>((width_ + 7) / 8);
    pixels_.assign(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_) * 4, 0);
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const std::uint8_t byte = bits_[static_cast<std::size_t>(y) * rowBytes +
                                            static_cast<std::size_t>(x / 8)];
            const bool set = (byte >> (x % 8)) & 0x01;  // XBM: LSB first
            std::uint8_t* dst = pixels_.data() + (static_cast<std::size_t>(y) * width_ + x) * 4;
            if (set) {
                if (foreground_) {
                    dst[0] = foreground_->r;
                    dst[1] = foreground_->g;
                    dst[2] = foreground_->b;
                    dst[3] = foreground_->a;
                }
            } else if (background_) {
                dst[0] = background_->r;
                dst[1] = background_->g;
                dst[2] = background_->b;
                dst[3] = background_->a;
            }  // else transparent
        }
    }
}

void BitmapItem::Draw(Renderer& renderer, Path&) const {
    if (IsHidden()) return;
    if (pixels_.empty() || width_ <= 0 || height_ <= 0) return;

    const double w = static_cast<double>(width_);
    const double h = static_cast<double>(height_);
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
