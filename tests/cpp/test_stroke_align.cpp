// test_stroke_align.cpp — unit tests for the pixel grid alignment of strokes.
#include <cassert>
#include <cmath>
#include <cstdio>

#include "tkd2dcanvas/StrokeAlign.h"

using tkd2dcanvas::AlignStrokePath;

static bool Near(double a, double b) { return std::fabs(a - b) < 1e-9; }

static Path Segment(double x1, double y1, double x2, double y2) {
    Path path;
    path.closed = false;
    path.Add({x1, y1});
    path.Add({x2, y2});
    return path;
}

static Path Rectangle(double x1, double y1, double x2, double y2) {
    Path path;
    path.closed = true;
    path.Add({x1, y1});
    path.Add({x2, y1});
    path.Add({x2, y2});
    path.Add({x1, y2});
    return path;
}

int main() {
    // A horizontal round capped line with whole pixel end points is moved on
    // both axes: the stroke edges and the cap tips land on the pixel grid.
    {
        Path in = Segment(27, 47, 359, 47);
        Path out;
        assert(AlignStrokePath(in, 9, CapStyle::Round, true, out));
        assert(Near(out.points[0].x, 27.5) && Near(out.points[0].y, 47.5));
        assert(Near(out.points[1].x, 359.5) && Near(out.points[1].y, 47.5));
    }

    // The line keeps its length: both end points move by the same amount.
    {
        Path in = Segment(27, 47, 359, 47);
        Path out;
        assert(AlignStrokePath(in, 9, CapStyle::Round, true, out));
        assert(Near(out.points[1].x - out.points[0].x, 332.0));
    }

    // A nested pair of polygons (TinUI's entry frame) is moved by the same
    // amount, so that the line above stays centred on the frame.
    {
        Path frame = Rectangle(26, 26, 360, 47);
        Path inner = Rectangle(27, 27, 359, 46);
        Path line = Segment(27, 47, 359, 47);
        Path frameOut, innerOut, lineOut;
        assert(AlignStrokePath(frame, 9, CapStyle::Round, true, frameOut));
        assert(AlignStrokePath(inner, 9, CapStyle::Round, true, innerOut));
        assert(AlignStrokePath(line, 9, CapStyle::Round, true, lineOut));
        // The line end points sit on the corners of the inner polygon.
        assert(Near(lineOut.points[0].x, innerOut.points[3].x));
        assert(Near(lineOut.points[1].x, innerOut.points[1].x));
    }

    // A flat capped line ends at its end points, so an end point that is
    // already on the pixel grid must not be moved.
    {
        Path in = Segment(10, 20, 50, 20);
        Path out;
        assert(AlignStrokePath(in, 1, CapStyle::Flat, true, out));
        assert(Near(out.points[0].x, 10.0));
        assert(Near(out.points[0].y, 20.5));
    }

    // A round cap reaches half the pen width past the end point, so the end
    // point is moved as well to keep the tip on the pixel grid.
    {
        Path in = Segment(10, 20, 50, 20);
        Path out;
        assert(AlignStrokePath(in, 1, CapStyle::Round, true, out));
        assert(Near(out.points[0].x, 10.5));
    }

    // Round joins (the ends of a two point polygon) are not moved along the
    // segment: only the axis perpendicular to it is aligned.
    {
        Path in = Segment(10, 20, 50, 20);
        Path out;
        assert(AlignStrokePath(in, 1, CapStyle::Round, false, out));
        assert(Near(out.points[0].x, 10.0));
        assert(Near(out.points[0].y, 20.5));
    }

    // A zero length segment has no direction and is not moved along it.
    {
        Path in = Segment(10, 20, 10, 20);
        Path out;
        assert(AlignStrokePath(in, 3, CapStyle::Round, true, out));
        assert(Near(out.points[0].x, 10.0));
        assert(Near(out.points[0].y, 20.5));
    }

    // A vertical segment with a fractional end point (a scrollbar thumb
    // spanning fractional view fractions) cannot be aligned along its own
    // axis, but its stroke edges still can.
    {
        Path in = Segment(245, 169, 245, 196.918);
        Path out;
        assert(AlignStrokePath(in, 3, CapStyle::Round, true, out));
        assert(Near(out.points[0].x, 245.5));
        assert(Near(out.points[0].y, 169.0));
        assert(Near(out.points[1].y, 196.918));
    }

    // Diagonal and curved paths are never aligned.
    {
        Path in = Segment(0, 0, 10, 10);
        Path out;
        assert(!AlignStrokePath(in, 3, CapStyle::Round, true, out));
    }

    // A fractional pen width cannot be aligned.
    {
        Path in = Segment(0, 0, 10, 0);
        Path out;
        assert(!AlignStrokePath(in, 2.5, CapStyle::Round, true, out));
    }

    // A polygon whose vertices do not share a phase is left alone instead of
    // being distorted.
    {
        Path in = Rectangle(26, 26, 360.5, 47);
        Path out;
        assert(!AlignStrokePath(in, 9, CapStyle::Round, true, out));
    }

    // A polygon whose edges already are on the pixel grid is not moved.
    {
        Path in = Rectangle(26, 26.5, 360, 47.5);
        Path out;
        assert(!AlignStrokePath(in, 9, CapStyle::Round, true, out));
    }

    std::puts("test_stroke_align: OK");
    return 0;
}
