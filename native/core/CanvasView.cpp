#include "tkd2dcanvas/CanvasView.h"

#include <algorithm>
#include <cmath>

void CanvasView::SetOffset(double x, double y) {
    if (confine_ && scrollRegion_.IsValid()) {
        const double maxX = std::max(0.0, scrollRegion_.Width() - viewWidth_);
        const double maxY = std::max(0.0, scrollRegion_.Height() - viewHeight_);
        x = std::clamp(x, 0.0, maxX);
        y = std::clamp(y, 0.0, maxY);
    }
    xOffset_ = x;
    yOffset_ = y;
}

void CanvasView::ScrollToFraction(bool horizontal, double fraction) {
    fraction = std::clamp(fraction, 0.0, 1.0);
    if (!scrollRegion_.IsValid()) return;

    if (horizontal) {
        const double span = std::max(0.0, scrollRegion_.Width() - viewWidth_);
        SetOffset(scrollRegion_.left + fraction * span, yOffset_);
    } else {
        const double span = std::max(0.0, scrollRegion_.Height() - viewHeight_);
        SetOffset(xOffset_, scrollRegion_.top + fraction * span);
    }
}

CanvasView::Fraction CanvasView::XViewFraction() const {
    Fraction f;
    if (!scrollRegion_.IsValid()) return f;

    const double total = scrollRegion_.Width();
    if (total <= 0.0) return f;

    f.first = (xOffset_ - scrollRegion_.left) / total;
    f.last = (xOffset_ + viewWidth_ - scrollRegion_.left) / total;
    f.first = std::clamp(f.first, 0.0, 1.0);
    f.last = std::clamp(f.last, 0.0, 1.0);
    return f;
}

CanvasView::Fraction CanvasView::YViewFraction() const {
    Fraction f;
    if (!scrollRegion_.IsValid()) return f;

    const double total = scrollRegion_.Height();
    if (total <= 0.0) return f;

    f.first = (yOffset_ - scrollRegion_.top) / total;
    f.last = (yOffset_ + viewHeight_ - scrollRegion_.top) / total;
    f.first = std::clamp(f.first, 0.0, 1.0);
    f.last = std::clamp(f.last, 0.0, 1.0);
    return f;
}
