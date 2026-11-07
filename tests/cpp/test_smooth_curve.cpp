// test_smooth_curve.cpp — Tk -smooth spline generator (tkTrig.c port).
#include "tkd2dcanvas/SmoothCurve.h"

#include <cmath>
#include <cstdio>
#include <vector>

static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        ++checks;                                                              \
        if (!(cond)) {                                                         \
            std::printf("FAIL %d: %s\n", __LINE__, #cond);                     \
            ++failures;                                                        \
        }                                                                      \
    } while (0)

static bool Close(double a, double b, double eps = 1e-9) { return std::fabs(a - b) <= eps; }

int main() {
    // ClampSplineSteps: Tk clamps -splinesteps to [1, 100].
    CHECK(ClampSplineSteps(0) == 1);
    CHECK(ClampSplineSteps(-5) == 1);
    CHECK(ClampSplineSteps(1) == 1);
    CHECK(ClampSplineSteps(12) == 12);
    CHECK(ClampSplineSteps(100) == 100);
    CHECK(ClampSplineSteps(101) == 100);
    CHECK(ClampSplineSteps(1000000) == 100);

    // Open 3-point curve, steps=2: A(0,0) B(10,0) C(20,0).
    // Tk: first spline of an open curve has control points
    //   (0,0) (6.667,0) (13.333,0) (20,0); at t=0.5 the curve is at x=10.
    const std::vector<Point> open3 = {{0, 0}, {10, 0}, {20, 0}};
    SmoothOptions bezier2{SmoothMode::Bezier, 2};
    std::vector<Point> out = SmoothCurvePoints(open3, bezier2);
    CHECK(out.size() == 1 + 1 * 2);  // seed + (m-2) * steps
    CHECK(out.front() == open3.front());
    CHECK(out.back() == open3.back());
    CHECK(Close(out[1].x, 10.0, 0.01) && Close(out[1].y, 0.0, 0.01));  // Tk's 0.333/0.667 constants

    // Open curve with steps=12: point count follows 1 + (m-2)*steps.
    const std::vector<Point> open5 = {{0, 0}, {10, -8}, {20, 0}, {30, 8}, {40, 0}};
    SmoothOptions bezier12{SmoothMode::Bezier, 12};
    out = SmoothCurvePoints(open5, bezier12);
    CHECK(out.size() == 1 + (open5.size() - 2) * 12);
    CHECK(out.front() == open5.front());
    CHECK(out.back() == open5.back());

    // Closed Tk coordinate list (first == last, polygon autoClosed form).
    const std::vector<Point> closed = {{0, 0}, {10, 0}, {10, 10}, {0, 0}};
    out = SmoothCurvePoints(closed, bezier2);
    CHECK(out.size() == 1 + (closed.size() - 1) * 2);  // 7
    CHECK(Close(out.front().x, 5.0) && Close(out.front().y, 5.0));
    // The closed spline ends where it starts.
    CHECK(out.back() == out.front());

    // Degenerate open curve: repeated point collapses to a single segment point.
    const std::vector<Point> dup = {{0, 0}, {0, 0}, {10, 0}};
    out = SmoothCurvePoints(dup, bezier2);
    CHECK(out.size() == 2);
    CHECK((out.back() == Point{10.0, 0.0}));

    // Raw mode: 4 points = one cubic bezier segment -> 1 knot + steps.
    const std::vector<Point> raw4 = {{0, 0}, {10, 0}, {10, 10}, {0, 10}};
    SmoothOptions raw{SmoothMode::Raw, 4};
    out = SmoothCurvePoints(raw4, raw);
    CHECK(out.size() == 1 + 4);
    CHECK(out.front() == raw4.front());
    CHECK(Close(out.back().x, 0.0) && Close(out.back().y, 10.0));

    // Raw mode, 5 points (one full segment + one shared knot):
    // 1 knot + steps for the first segment + steps for the wrap-around segment.
    const std::vector<Point> raw5 = {{0, 0}, {10, 0}, {10, 10}, {0, 10}, {10, 20}};
    out = SmoothCurvePoints(raw5, raw);
    CHECK(out.size() == 1 + 4 + 4);

    // None mode returns the input untouched.
    SmoothOptions none{SmoothMode::None, 12};
    out = SmoothCurvePoints(open5, none);
    CHECK(out == open5);

    std::printf("test_smooth_curve: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
