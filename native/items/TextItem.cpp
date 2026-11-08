#include "TextItem.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "tkd2dcanvas/BBox.h"

TextItem::TextItem(Id id, double x, double y, std::string text)
    : CanvasItem(id, "text"), x_(x), y_(y), text_(std::move(text)) {
    // Tk canvas text defaults to black fill.
    Style().fill = Color{0, 0, 0};
}

void TextItem::SetLayout(TextBlockLayout layout) {
    if (layout.lines.empty()) layout.lines = {""};
    if (layout.lineWidths.size() != layout.lines.size()) {
        layout.lineWidths.assign(layout.lines.size(), 0.0);
    }
    double maxWidth = 0.0;
    for (const double w : layout.lineWidths) maxWidth = std::max(maxWidth, w);
    layout.maxLineWidth = maxWidth;
    layout_ = std::move(layout);
}

const double kMinBBoxSpan = 1.0;

void TextItem::SetLineHeight(double height) {
    if (height > 0.0) lineHeight_ = height;
}

void TextItem::SetAngle(double angle) {
    double r = std::fmod(angle, 360.0);
    if (r < 0.0) r += 360.0;
    if (r == 0.0) r = 0.0;
    angle_ = r;
    const double rad = angle_ * (3.14159265358979323846 / 180.0);
    sine_ = std::sin(rad);
    cosine_ = std::cos(rad);
}

BBox TextItem::GetBBox() const {
    if (IsHidden()) return BBox::Empty();
    const double w = std::max(layout_.maxLineWidth, kMinBBoxSpan);
    const double h = std::max(BlockHeight(), kMinBBoxSpan);
    const double offsetX = AnchorOffsetX(anchor_, w);
    const double offsetY = AnchorOffsetY(anchor_, h);

    constexpr double kTextBBoxFudge = 1.0;
    const double corners[4][2] = {
        {offsetX - kTextBBoxFudge, offsetY},
        {offsetX + w + kTextBBoxFudge, offsetY},
        {offsetX + w + kTextBBoxFudge, offsetY + h},
        {offsetX - kTextBBoxFudge, offsetY + h},
    };

    if (angle_ == 0.0) {
        // No rotation: corners are directly the min/max.
        return BBox{TkRoundHalf(x_ + corners[0][0]),
                    TkRoundHalf(y_ + corners[0][1]),
                    TkRoundHalf(x_ + corners[1][0]),
                    TkRoundHalf(y_ + corners[2][1])};
    }

    // Rotate the four block corners about the anchor point (x, y).
    const double s = sine_, c = cosine_;
    double minX = 0.0, minY = 0.0, maxX = 0.0, maxY = 0.0;
    for (int i = 0; i < 4; ++i) {
        const double dx = corners[i][0], dy = corners[i][1];
        const double rx = x_ + dx * c + dy * s;
        const double ry = y_ + dy * c - dx * s;
        if (i == 0) {
            minX = maxX = rx;
            minY = maxY = ry;
        } else {
            minX = std::min(minX, rx);
            maxX = std::max(maxX, rx);
            minY = std::min(minY, ry);
            maxY = std::max(maxY, ry);
        }
    }
    // Tk's ROUND() is floor(d + 0.5) for every value.
    return BBox{TkRoundHalf(minX), TkRoundHalf(minY),
                TkRoundHalf(maxX), TkRoundHalf(maxY)};
}

void TextItem::Translate(double dx, double dy) {
    x_ += dx;
    y_ += dy;
}

std::vector<double> TextItem::GetCoordinates() const { return {x_, y_}; }

bool TextItem::SetCoordinates(const std::vector<double>& coords) {
    if (coords.size() != 2) return false;
    x_ = coords[0];
    y_ = coords[1];
    return true;
}

double TextItem::DistanceToRect(double px, double py, const BBox& rect) {
    if (px >= rect.left && px <= rect.right && py >= rect.top && py <= rect.bottom) {
        return 0.0;
    }
    const double dx = (px < rect.left) ? rect.left - px
                     : (px > rect.right) ? px - rect.right
                     : 0.0;
    const double dy = (py < rect.top) ? rect.top - py
                     : (py > rect.bottom) ? py - rect.bottom
                     : 0.0;
    return std::sqrt(dx * dx + dy * dy);
}

double TextItem::DistanceTo(Point p) const {
    const double blockW = layout_.maxLineWidth;
    const double lineHeight = LineHeight();
    const double blockH = BlockHeight();

    Point local = p;
    if (angle_ != 0.0) {
        const double offsetX = AnchorOffsetX(anchor_, blockW);
        const double offsetY = AnchorOffsetY(anchor_, blockH);
        const double ox = x_ + offsetX * cosine_ + offsetY * sine_;
        const double oy = y_ + offsetY * cosine_ - offsetX * sine_;
        const double px = p.x - ox, py = p.y - oy;
        const double s = sine_, c = cosine_;
        local = Point{c * px - s * py, s * px + c * py};
    } else {
        const Point topLeft = AnchorTopLeft(anchor_, x_, y_, blockW, blockH);
        local = Point{p.x - topLeft.x, p.y - topLeft.y};
    }

    double best = 0.0;
    bool any = false;
    const std::size_t n = layout_.lines.size();
    for (std::size_t i = 0; i < n; ++i) {
        const double lineW = i < layout_.lineWidths.size() ? layout_.lineWidths[i] : 0.0;
        const double left = JustifyOffset(justify_, blockW, lineW);
        const BBox lineRect{left, static_cast<double>(i) * lineHeight,
                            left + lineW, static_cast<double>(i + 1) * lineHeight};
        const double d = DistanceToRect(local.x, local.y, lineRect);
        if (!any || d < best) best = d;
        any = true;
    }

    if (any) return best;
    if (angle_ != 0.0) return std::hypot(local.x, local.y);
    return std::hypot(p.x - x_, p.y - y_);
}

void TextItem::Draw(Renderer& renderer) const {
    if (IsHidden()) return;
    if (layout_.lines.empty()) return;

    if (!Style().fill) return;  // -fill "" disables the text (Tk)

    const Color fill = (Style().state == ItemState::Disabled && Style().disabledFill.has_value())
        ? *Style().disabledFill
        : *Style().fill;

    TextStyle style;
    style.fontFamily = font_.family;
    style.size = font_.size;
    style.bold = font_.bold;
    style.italic = font_.italic;
    style.underline = font_.underline;
    style.overstrike = font_.overstrike;
    style.color = fill;

    TextLayout layout;
    layout.lines = layout_.lines;
    layout.origin = Point{x_, y_};
    layout.anchor = anchor_;
    layout.justify = justify_;
    layout.blockWidth = layout_.maxLineWidth;
    layout.lineSpacing = LineHeight();
    layout.angle = angle_;
    layout.lineWidths = layout_.lineWidths;
    renderer.drawText(layout, style);
}
