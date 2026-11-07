#include "tkd2dcanvas/Transform2D.h"

#include <cmath>

namespace {
constexpr double kPi = 3.14159265358979323846;
}

Transform2D Transform2D::Translate(double dx, double dy) {
    return FromValues(1.0, 0.0, 0.0, 1.0, dx, dy);
}

Transform2D Transform2D::Scale(double sx, double sy) {
    return FromValues(sx, 0.0, 0.0, sy, 0.0, 0.0);
}

Transform2D Transform2D::Rotate(double radians) {
    const double c = std::cos(radians);
    const double s = std::sin(radians);
    return FromValues(c, s, -s, c, 0.0, 0.0);
}

Transform2D Transform2D::Multiply(const Transform2D& rhs) const {
    // this * rhs  with  m = [a c e; b d f]
    return FromValues(
        a_ * rhs.a_ + c_ * rhs.b_,
        b_ * rhs.a_ + d_ * rhs.b_,
        a_ * rhs.c_ + c_ * rhs.d_,
        b_ * rhs.c_ + d_ * rhs.d_,
        a_ * rhs.e_ + c_ * rhs.f_ + e_,
        b_ * rhs.e_ + d_ * rhs.f_ + f_);
}

Point Transform2D::Apply(Point p) const {
    return Point{a_ * p.x + c_ * p.y + e_, b_ * p.x + d_ * p.y + f_};
}

BBox Transform2D::Apply(const BBox& box) const {
    if (!box.IsValid()) return BBox::Empty();

    const Point corners[4] = {
        {box.left, box.top},
        {box.right, box.top},
        {box.right, box.bottom},
        {box.left, box.bottom},
    };

    double minX = 0.0, minY = 0.0, maxX = 0.0, maxY = 0.0;
    for (std::size_t i = 0; i < 4; ++i) {
        const Point q = Apply(corners[i]);
        if (i == 0) {
            minX = maxX = q.x;
            minY = maxY = q.y;
        } else {
            minX = std::min(minX, q.x);
            minY = std::min(minY, q.y);
            maxX = std::max(maxX, q.x);
            maxY = std::max(maxY, q.y);
        }
    }
    return BBox{minX, minY, maxX, maxY};
}

bool Transform2D::IsIdentity() const {
    return a_ == 1.0 && b_ == 0.0 && c_ == 0.0 && d_ == 1.0 && e_ == 0.0 && f_ == 0.0;
}
