// SmoothCurve.cpp — direct port of Tk's spline generators (tkTrig.c).
// Input/output are point lists (Tk works on flat double* arrays). Steps:
//   TkBezierPoints(control, numSteps, out) == emit numSteps cubic-bezier
//   samples for t in (0, 1] (the start point is the caller's business).
#include "tkd2dcanvas/SmoothCurve.h"

#include <cmath>

int ClampSplineSteps(int steps) {
    if (steps < kMinSplineSteps) return kMinSplineSteps;
    if (steps > kMaxSplineSteps) return kMaxSplineSteps;
    return steps;
}

namespace {

bool SamePoint(const Point& a, const Point& b) {
    return a.x == b.x && a.y == b.y;
}

// Port of TkBezierPoints: emits numSteps points for t = i/numSteps (i = 1..numSteps).
void EmitBezierPoints(const double control[8], int numSteps, std::vector<Point>& out) {
    for (int i = 1; i <= numSteps; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(numSteps);
        const double t2 = t * t;
        const double t3 = t2 * t;
        const double u = 1.0 - t;
        const double u2 = u * u;
        const double u3 = u2 * u;
        out.push_back(Point{control[0] * u3 + 3.0 * (control[2] * t * u2 + control[4] * t2 * u) + control[6] * t3,
                            control[1] * u3 + 3.0 * (control[3] * t * u2 + control[5] * t2 * u) + control[7] * t3});
    }
}

std::vector<Point> MakeBezierCurve(const std::vector<Point>& point, int numSteps) {
    std::vector<Point> out;
    const std::size_t m = point.size();
    if (m == 0) return out;
    out.reserve(1 + m * static_cast<std::size_t>(numSteps));

    // A closed curve needs the wrapped-around spline, which reads tkPoints[2];
    // guard against degenerate 2-point inputs.
    const bool closed = m >= 3 && SamePoint(point.front(), point.back());

    // If the curve is closed, generate a spline spanning the last and first
    // points; otherwise seed the output with the first point.
    if (closed) {
        double control[8];
        // tkPoints[n-1] == tkPoints[n], so the "second-to-last" vertex is m-2.
        control[0] = 0.5 * point[m - 2].x + 0.5 * point[0].x;
        control[1] = 0.5 * point[m - 2].y + 0.5 * point[0].y;
        control[2] = 0.167 * point[m - 2].x + 0.833 * point[0].x;
        control[3] = 0.167 * point[m - 2].y + 0.833 * point[0].y;
        control[4] = 0.833 * point[0].x + 0.167 * point[2].x;
        control[5] = 0.833 * point[0].y + 0.167 * point[2].y;
        control[6] = 0.5 * point[0].x + 0.5 * point[2].x;
        control[7] = 0.5 * point[0].y + 0.5 * point[2].y;
        out.push_back(Point{control[0], control[1]});
        EmitBezierPoints(control, numSteps, out);
    } else {
        out.push_back(point[0]);
    }

    for (std::size_t i = 2; i < m; ++i) {
        const Point& p0 = point[i - 2];
        const Point& p1 = point[i - 1];
        const Point& p2 = point[i];
        double control[8];

        // First two control points; the first spline of an open curve is
        // special-cased.
        if (i == 2 && !closed) {
            control[0] = p0.x;
            control[1] = p0.y;
            control[2] = 0.333 * p0.x + 0.667 * p2.x;
            control[3] = 0.333 * p0.y + 0.667 * p2.y;
        } else {
            control[0] = 0.5 * p0.x + 0.5 * p1.x;
            control[1] = 0.5 * p0.y + 0.5 * p1.y;
            control[2] = 0.167 * p0.x + 0.833 * p1.x;
            control[3] = 0.167 * p0.y + 0.833 * p1.y;
        }

        // Last two control points; the last spline of an open curve is
        // special-cased.
        if (i == m - 1 && !closed) {
            control[4] = 0.667 * p1.x + 0.333 * p2.x;
            control[5] = 0.667 * p1.y + 0.333 * p2.y;
            control[6] = p2.x;
            control[7] = p2.y;
        } else {
            control[4] = 0.833 * p1.x + 0.167 * p2.x;
            control[5] = 0.833 * p1.y + 0.167 * p2.y;
            control[6] = 0.5 * p1.x + 0.5 * p2.x;
            control[7] = 0.5 * p1.y + 0.5 * p2.y;
        }

        // If either pair of neighbouring points coincides, emit a single
        // straight-line point instead of a degenerate curve.
        if (SamePoint(p0, p1) || SamePoint(p1, p2)) {
            out.push_back(Point{control[6], control[7]});
            continue;
        }

        EmitBezierPoints(control, numSteps, out);
    }
    return out;
}

std::vector<Point> MakeRawCurve(const std::vector<Point>& point, int numSteps) {
    std::vector<Point> out;
    const std::size_t numPoints = point.size();
    if (numPoints == 0) return out;
    const std::size_t numSegments = (numPoints + 1) / 3;
    out.reserve(1 + numSegments * static_cast<std::size_t>(numSteps));

    out.push_back(point[0]);

    // All segments except the one that overlaps the end of the list.
    std::size_t i = numPoints;   // remaining points, including knot + controls
    std::size_t seg = 0;         // index of the current segment's first point
    while (i >= 4) {
        const double control[8] = {point[seg + 0].x, point[seg + 0].y,
                                   point[seg + 1].x, point[seg + 1].y,
                                   point[seg + 2].x, point[seg + 2].y,
                                   point[seg + 3].x, point[seg + 3].y};
        if (control[0] == control[2] && control[1] == control[3] &&
            control[4] == control[6] && control[5] == control[7]) {
            // Degenerate segment: straight line between knots.
            out.push_back(Point{control[6], control[7]});
        } else {
            EmitBezierPoints(control, numSteps, out);
        }
        i -= 3;
        seg += 3;
    }

    if (i > 1) {
        // One more segment wrapping around the end of the list: reuse the
        // first points as trailing control points (Tk's "wrap-around" case).
        double control[8];
        std::size_t j = 0;
        for (; j < 2 * i; ++j) {
            const std::size_t src = seg + j / 2;
            control[j] = (j % 2 == 0) ? point[src].x : point[src].y;
        }
        for (; j < 8; ++j) {
            const std::size_t src = (j - 2 * i) / 2;
            control[j] = ((j - 2 * i) % 2 == 0) ? point[src].x : point[src].y;
        }

        if (control[0] == control[2] && control[1] == control[3] &&
            control[4] == control[6] && control[5] == control[7]) {
            out.push_back(Point{control[6], control[7]});
        } else {
            EmitBezierPoints(control, numSteps, out);
        }
    }
    return out;
}

}  // namespace

std::vector<Point> SmoothCurvePoints(const std::vector<Point>& tkPoints,
                                     const SmoothOptions& options) {
    if (tkPoints.empty() || options.mode == SmoothMode::None) return tkPoints;
    const int steps = ClampSplineSteps(options.splineSteps);
    switch (options.mode) {
        case SmoothMode::Bezier: return MakeBezierCurve(tkPoints, steps);
        case SmoothMode::Raw:    return MakeRawCurve(tkPoints, steps);
        default:                 return tkPoints;
    }
}
