// SmoothCurve.h — Tk's "-smooth" spline generation (tkTrig.c).
// Pure geometry; no platform dependencies. The algorithm is a direct port of
// TkMakeBezierCurve ("smooth true", parabolic splines fitted to the segments)
// and TkMakeRawCurve ("smooth raw", cubic bezier knots/control points).
// See third_party/tcltk-src/tk/generic/tkTrig.c.
#pragma once

#include <vector>

#include "tkd2dcanvas/Geometry.h"

// Tk clamps -splinesteps to [1, 100]; the default is 12.
constexpr int kMinSplineSteps = 1;
constexpr int kMaxSplineSteps = 100;
constexpr int kDefaultSplineSteps = 12;

int ClampSplineSteps(int steps);

// Which -smooth method is active.
enum class SmoothMode {
    None = 0,   // straight segments (-smooth 0)
    Bezier = 1, // Tk's default smooth ("true"/"bezier"): parabolic splines
    Raw = 2,    // "smooth raw": points are cubic bezier knots/control points
};

// Shared "-smooth"/"-splinesteps" state for line/polygon items.
struct SmoothOptions {
    SmoothMode mode = SmoothMode::None;
    int splineSteps = kDefaultSplineSteps;

    bool operator==(const SmoothOptions& o) const {
        return mode == o.mode && splineSteps == o.splineSteps;
    }
    bool operator!=(const SmoothOptions& o) const { return !(*this == o); }
};

// Generates the flattened curve for Tk-style *tkPoints* (flat coordinate list
// as point pairs). Mirrors the Tk contract:
//   - A closed curve is produced when the first and last points coincide.
//   - Bezier mode: each spline segment emits `numSteps` points (plus the
//     initial point, emitted by the caller's Tk-closed branch or as p[0]).
//   - Raw mode: the input is (3n+1), 3n, or 3n-1 points describing n cubic
//     bezier segments.
// Callers pass the polygon's coordinates with the closing point duplicated
// (Tk's autoClosed behavior) to get the wrapped-around closed spline.
std::vector<Point> SmoothCurvePoints(const std::vector<Point>& tkPoints,
                                     const SmoothOptions& options);
