#include "TextItem.h"

#include <utility>

TextItem::TextItem(Id id, double x, double y, std::string text)
    : CanvasItem(id, "text"), x_(x), y_(y), text_(std::move(text)) {}

BBox TextItem::GetBBox() const {
    // Cheap estimate until real font metrics are wired in (M9).
    const double width = fontSize_ * 0.6 * static_cast<double>(text_.size());
    const double height = fontSize_ * 1.25;
    return BBox{x_, y_, x_ + width, y_ + height};
}

void TextItem::Translate(double dx, double dy) {
    x_ += dx;
    y_ += dy;
}

void TextItem::Draw(Renderer&) const {
    // TODO(M9): build TextLayout from -text/-font/-anchor/-justify and call
    // renderer.drawText().
}
