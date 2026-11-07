// BBox.h — axis-aligned bounding box. No external dependencies.
#pragma once

#include <algorithm>

// (std::min/std::max are intentionally used nowhere in this header.)

struct BBox {
    double left = 0.0;
    double top = 0.0;
    double right = 0.0;
    double bottom = 0.0;

    static BBox Empty() { return BBox{0.0, 0.0, -1.0, -1.0}; }

    double Width() const { return right - left; }
    double Height() const { return bottom - top; }
    double CenterX() const { return left + Width() * 0.5; }
    double CenterY() const { return top + Height() * 0.5; }

    bool IsValid() const { return left <= right && top <= bottom; }

    bool Intersects(const BBox& o) const {
        if (!IsValid() || !o.IsValid()) return false;
        return left <= o.right && right >= o.left && top <= o.bottom && bottom >= o.top;
    }

    bool Contains(const BBox& o) const {
        if (!IsValid() || !o.IsValid()) return false;
        return left <= o.left && top <= o.top && right >= o.right && bottom >= o.bottom;
    }

    BBox Union(const BBox& o) const {
        if (!o.IsValid()) return *this;
        if (!IsValid()) return o;
        // NOTE: no std::min/std::max here — this header may be included after
        // <windows.h> without NOMINMAX; avoid the min/max macros.
        const double l = (left < o.left) ? left : o.left;
        const double t = (top < o.top) ? top : o.top;
        const double r = (right > o.right) ? right : o.right;
        const double b = (bottom > o.bottom) ? bottom : o.bottom;
        return BBox{l, t, r, b};
    }

    void Inflate(double dx, double dy) {
        if (!IsValid()) return;
        left -= dx;
        top -= dy;
        right += dx;
        bottom += dy;
    }

    bool operator==(const BBox& o) const {
        return left == o.left && top == o.top && right == o.right && bottom == o.bottom;
    }
    bool operator!=(const BBox& o) const { return !(*this == o); }
};
