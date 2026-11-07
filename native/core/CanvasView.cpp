#include "tkd2dcanvas/CanvasView.h"

#include <algorithm>
#include <cmath>

void CanvasView::SetScrollRegion(const BBox& region) {
    scrollRegion_ = region;
    // A region change re-confines the view (Tk: ConfigureCanvas ends with
    // CanvasSetOrigin).
    SetOrigin(xOffset_, yOffset_);
}

void CanvasView::SetViewSize(int width, int height) {
    viewWidth_ = std::max(0, width);
    viewHeight_ = std::max(0, height);
    // A resize may leave the view sticking out of the region (Tk: the
    // ConfigureNotify handler recenters a confined canvas whose scroll
    // region is smaller than the window).
    SetOrigin(xOffset_, yOffset_);
}

// Tk (tkCanvas.c, CanvasSetOrigin) rounds the origin to the nearest
// multiple of the increment; ties go away from zero. The sign branches are
// Tk's own, with fmod in place of C's truncated %.
double CanvasView::RoundToIncrement(double value, double increment) {
    if (increment <= 0.0) return value;
    if (value >= 0.0) {
        value += increment * 0.5;
        return value - std::fmod(value, increment);
    }
    value = -value + increment * 0.5;
    return -(value - std::fmod(value, increment));
}

void CanvasView::ConfineOrigin(double& x, double& y) const {
    if (!confine_ || !scrollRegion_.IsValid()) return;

    // Direct translation of Tk's CanvasSetOrigin confinement (inset = 0):
    // the view may not stick out past the region on the side where the
    // other side still fits inside; otherwise it is pulled back by just
    // enough to stop sticking out.
    const double left = x - scrollRegion_.left;
    const double right = scrollRegion_.right - (x + viewWidth_);
    if ((left < 0.0) && (right > 0.0)) {
        double delta = (right > -left) ? -left : right;
        if (xIncrement_ > 0.0) delta -= std::fmod(delta, xIncrement_);
        x += delta;
    } else if ((right < 0.0) && (left > 0.0)) {
        double delta = (left > -right) ? -right : left;
        if (xIncrement_ > 0.0) delta -= std::fmod(delta, xIncrement_);
        x -= delta;
    }

    const double top = y - scrollRegion_.top;
    const double bottom = scrollRegion_.bottom - (y + viewHeight_);
    if ((top < 0.0) && (bottom > 0.0)) {
        double delta = (bottom > -top) ? -top : bottom;
        if (yIncrement_ > 0.0) delta -= std::fmod(delta, yIncrement_);
        y += delta;
    } else if ((bottom < 0.0) && (top > 0.0)) {
        double delta = (top > -bottom) ? -bottom : top;
        if (yIncrement_ > 0.0) delta -= std::fmod(delta, yIncrement_);
        y -= delta;
    }
}

void CanvasView::SetOrigin(double x, double y) {
    x = RoundToIncrement(x, xIncrement_);
    y = RoundToIncrement(y, yIncrement_);
    ConfineOrigin(x, y);
    xOffset_ = x;
    yOffset_ = y;
}

void CanvasView::MoveTo(bool horizontal, double fraction) {
    // Tk: newOrigin = scrollX1 - inset + (int) (fraction*(scrollX2-scrollX1)
    // + 0.5); the (int) cast truncates toward zero. The fraction is not
    // clamped: confine pulls out-of-range values back. Without a region
    // Tk computes scrollX1 + fraction*0 = 0.
    fraction = std::clamp(fraction, -1.0e15, 1.0e15);  // keep the int cast defined
    if (horizontal) {
        double newX = 0.0;
        if (scrollRegion_.IsValid()) {
            const double width = scrollRegion_.Width();
            newX = scrollRegion_.left + static_cast<double>(static_cast<int>(fraction * width + 0.5));
        }
        SetOrigin(newX, yOffset_);
    } else {
        double newY = 0.0;
        if (scrollRegion_.IsValid()) {
            const double height = scrollRegion_.Height();
            newY = scrollRegion_.top + static_cast<double>(static_cast<int>(fraction * height + 0.5));
        }
        SetOrigin(xOffset_, newY);
    }
}

void CanvasView::ScrollUnits(bool horizontal, int count) {
    const double view = static_cast<double>(horizontal ? viewWidth_ : viewHeight_);
    const double increment = horizontal ? xIncrement_ : yIncrement_;
    const double base = horizontal ? xOffset_ : yOffset_;
    const double newOff = (increment > 0.0)
        ? base + static_cast<double>(count) * increment
        : base + static_cast<double>(count) * 0.1 * view;
    if (horizontal) SetOrigin(newOff, yOffset_); else SetOrigin(xOffset_, newOff);
}

void CanvasView::ScrollPages(bool horizontal, int count) {
    const double view = static_cast<double>(horizontal ? viewWidth_ : viewHeight_);
    const double base = horizontal ? xOffset_ : yOffset_;
    const double newOff = base + static_cast<double>(count) * 0.9 * view;
    if (horizontal) SetOrigin(newOff, yOffset_); else SetOrigin(xOffset_, newOff);
}

// Tk (tkCanvas.c, ScrollFractions): the visible span over the region;
// range <= 0 reports the full {0 1} span.
static CanvasView::Fraction ComputeFraction(double screen1, double screen2,
                                            double object1, double object2) {
    CanvasView::Fraction f;
    const double range = object2 - object1;
    if (range <= 0.0) return f;  // {0.0, 1.0}

    f.first = (screen1 - object1) / range;
    if (f.first < 0.0) f.first = 0.0;
    f.last = (screen2 - object1) / range;
    if (f.last > 1.0) f.last = 1.0;
    if (f.last < f.first) f.last = f.first;
    return f;
}

CanvasView::Fraction CanvasView::XViewFraction() const {
    return ComputeFraction(xOffset_, xOffset_ + static_cast<double>(viewWidth_),
                           scrollRegion_.left, scrollRegion_.right);
}

CanvasView::Fraction CanvasView::YViewFraction() const {
    return ComputeFraction(yOffset_, yOffset_ + static_cast<double>(viewHeight_),
                           scrollRegion_.top, scrollRegion_.bottom);
}
