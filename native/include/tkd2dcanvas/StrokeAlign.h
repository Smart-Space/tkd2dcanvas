// StrokeAlign.h — moves a stroke onto the pixel grid.
//
// Tk's canvas draws shapes with aliased GDI strokes: a stroke covers whole
// pixels, and an edge that falls on a half pixel is resolved by sampling the
// pixel centre. Direct2D antialiases instead, so a stroke edge on a half
// pixel spills over two pixel rows with 50% coverage each. A 1px border drawn
// that way looks 2px wide and soft, while Tk draws it as one solid row - the
// classic example is TinUI's rounded button, which stacks a thick outline
// polygon and an inset face polygon to fake a 1px border.
//
// Shifting the stroke geometry by half a pixel puts the stroke edges back on
// the pixel grid: the coverage becomes 0/1 per pixel (crisp, like Tk) while
// curved and diagonal edges keep their antialiasing.
//
// The shift is decided per axis. An axis can only be moved when *all* the
// stroke edges on that axis share the same half pixel phase; otherwise the
// shape would be distorted.
//
// Items that are meant to line up have to be moved the same way. TinUI's
// entry draws its frame as two nested rounded polygons and its underline as
// a separate line: the underline only matches the frame when the line is
// moved along the segment as well, otherwise it sits half a pixel off centre
// and its two ends are cut differently.
//
// The end points of a segment are stroke edges too. A round cap reaches half
// the pen width past the end point, and its tip lands in the middle of a
// pixel when the end point sits on a whole pixel. Moving the segment along
// its own axis therefore also moves the cap tip onto the pixel grid.
//
// Only real caps are moved along the segment; the round ends of a two point
// polygon are joins and keep their place (see `capEnds` below).
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "tkd2dcanvas/Geometry.h"
#include "tkd2dcanvas/Renderer.h"

namespace tkd2dcanvas {

// The phase of a coordinate on the pixel grid: 0 for a whole pixel, 1 for a
// half pixel and -1 for anything in between (which cannot be aligned).
inline int PixelPhase(double value) {
    const double frac = value - std::floor(value);
    if (frac < 1e-9) return 0;
    if (std::abs(frac - 0.5) < 1e-9) return 1;
    return -1;
}

// The shift that moves the given stroke edges onto the pixel grid.
//
// Returns 0.5 when every edge sits on a half pixel (moving them by half a
// pixel lands them on the grid), and 0 when the edges already are on the
// grid or do not share a common phase.
inline double HalfPixelShift(const std::vector<double>& edges) {
    if (edges.empty()) return 0.0;
    const int phase = PixelPhase(edges.front());
    if (phase < 0) return 0.0;
    for (double edge : edges) {
        if (PixelPhase(edge) != phase) return 0.0;
    }
    return phase == 1 ? 0.5 : 0.0;
}

// Moves the path so that its stroke edges fall onto the pixel grid.
//
// Returns false (and leaves `out` untouched) when the path cannot be
// aligned: a diagonal or curved path, a fractional pen width, or an axis
// whose edges do not share a common half pixel phase.
//
// `capEnds` tells whether the ends of an open segment are real caps. A two
// point polygon draws a segment with round *joins* at its degenerate vertex
// (Tk's polygons have no -capstyle option), and such an end keeps its
// place: it is not moved along the segment, so that the round shape of the
// end stays exactly as it was drawn before.
//
// `edgesX` and `edgesY` are scratch buffers: the renderer owns them and
// reuses them, so that aligning a stroke does not allocate.
inline bool AlignStrokePath(const Path& path, double width, CapStyle cap,
                             bool capEnds, Path& out,
                             std::vector<double>& edgesX,
                             std::vector<double>& edgesY) {
    if (path.points.size() < 2 || width <= 0.0) return false;

    const double rounded = std::floor(width + 0.5);
    if (std::abs(rounded - width) > 1e-9) return false;  // fractional width
    const double half = rounded * 0.5;

    // The stroke edge coordinates of the path, per axis.
    edgesX.clear();
    edgesY.clear();

    if (!path.closed) {
        // Only a single straight segment can be moved onto the pixel grid.
        if (path.points.size() != 2) return false;
        const Point& a = path.points[0];
        const Point& b = path.points[1];
        if (a.y == b.y) {  // horizontal
            edgesY.push_back(a.y - half);
            edgesY.push_back(a.y + half);
            const double lo = std::min(a.x, b.x);
            const double hi = std::max(a.x, b.x);
            // A flat cap ends at the end point, a round or square cap reaches
            // half the pen width past it. A zero length segment has no
            // direction, so it is not moved along the segment either.
            if (capEnds && lo != hi) {
                if (cap == CapStyle::Flat) {
                    edgesX.push_back(lo);
                    edgesX.push_back(hi);
                } else {
                    edgesX.push_back(lo - half);
                    edgesX.push_back(hi + half);
                }
            }
        } else if (a.x == b.x) {  // vertical
            edgesX.push_back(a.x - half);
            edgesX.push_back(a.x + half);
            const double lo = std::min(a.y, b.y);
            const double hi = std::max(a.y, b.y);
            if (capEnds) {
                if (cap == CapStyle::Flat) {
                    edgesY.push_back(lo);
                    edgesY.push_back(hi);
                } else {
                    edgesY.push_back(lo - half);
                    edgesY.push_back(hi + half);
                }
            }
        } else {
            return false;  // diagonal
        }
    } else {
        if (path.points.size() < 3) return false;

        // A diagonal edge can't be aligned at all: keep the raw path.
        const std::size_t n = path.points.size();
        for (std::size_t i = 0; i < n; ++i) {
            const Point& p = path.points[i];
            const Point& next = path.points[(i + 1) % n];
            if (p.x != next.x && p.y != next.y) return false;  // diagonal edge
        }

        edgesX.reserve(n * 2);
        edgesY.reserve(n * 2);
        for (const Point& p : path.points) {
            edgesX.push_back(p.x - half);
            edgesX.push_back(p.x + half);
            edgesY.push_back(p.y - half);
            edgesY.push_back(p.y + half);
        }
    }

    const double dx = HalfPixelShift(edgesX);
    const double dy = HalfPixelShift(edgesY);
    if (dx == 0.0 && dy == 0.0) return false;

    out = path;
    for (Point& p : out.points) {
        p.x += dx;
        p.y += dy;
    }
    return true;
}

// The same, but with scratch buffers of its own.  Callers that align many
// strokes should use the overload above and keep the buffers.
inline bool AlignStrokePath(const Path& path, double width, CapStyle cap,
                             bool capEnds, Path& out) {
    std::vector<double> edgesX;
    std::vector<double> edgesY;
    return AlignStrokePath(path, width, cap, capEnds, out, edgesX, edgesY);
}

}  // namespace tkd2dcanvas
