// Transform2D.h — 2D affine transform, column-major-style coefficients
// matching Direct2D conventions (D2D1_MATRIX_3X2_F): [a c e; b d f; 0 0 1].
#pragma once

#include "tkd2dcanvas/Geometry.h"
#include "tkd2dcanvas/BBox.h"

class Transform2D {
public:
    Transform2D() = default;  // identity

    static Transform2D Identity() { return Transform2D{}; }
    static Transform2D FromValues(double a, double b, double c, double d, double e, double f) {
        Transform2D t;
        t.a_ = a; t.b_ = b; t.c_ = c; t.d_ = d; t.e_ = e; t.f_ = f;
        return t;
    }

    static Transform2D Translate(double dx, double dy);
    static Transform2D Scale(double sx, double sy);
    static Transform2D Rotate(double radians);

    // this * rhs (apply rhs first, then this)
    Transform2D Multiply(const Transform2D& rhs) const;
    Transform2D operator*(const Transform2D& rhs) const { return Multiply(rhs); }

    Point Apply(Point p) const;
    BBox Apply(const BBox& box) const;

    bool IsIdentity() const;

    // Raw coefficients matching D2D1_MATRIX_3X2_F layout: [a c e; b d f]
    void Coefficients(double& a, double& b, double& c, double& d, double& e, double& f) const {
        a = a_; b = b_; c = c_; d = d_; e = e_; f = f_;
    }

private:
    double a_ = 1.0, b_ = 0.0, c_ = 0.0, d_ = 1.0, e_ = 0.0, f_ = 0.0;
};
