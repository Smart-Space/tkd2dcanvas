// TkAreaOverlap.h — Tk-faithful "shape vs rectangle" area tests.
//
// Direct ports of the corresponding routines in tkTrig.c (Tk 8.6.18):
// TkLineToArea, TkPolygonToArea (+ TkPolygonToPoint), TkOvalToArea,
// TkThickPolyLineToArea and TkGetButtPoints.  They all return the same
// classification Tk uses for `find overlapping` / `find enclosed`:
//   -1 the shape is entirely outside the rectangle,
//    0 the shape overlaps the rectangle,
//    1 the shape is entirely inside the rectangle.
// The rectangle is given as {x1, y1, x2, y2} with x1 <= x2, y1 <= y2.
//
// These ports exist so that item types can classify themselves against the
// probe rectangle exactly like Tk's item areaProcs; line items use them.
#pragma once

#include <cstddef>
#include <vector>

#include "tkd2dcanvas/BBox.h"
#include "tkd2dcanvas/Geometry.h"
#include "tkd2dcanvas/ItemStyle.h"
#include "tkd2dcanvas/Renderer.h"  // CapStyle / JoinStyle

// Tk allocates the outline GC only when an outline color is set AND the
// width is non-zero (tkRectOval.c / tkCanvLine.c / tkCanvPoly.c:
// `if (mask && width != 0 && color != NULL)`). The GC's existence is what
// gates the pen-width bloat, the miter points, the hollow-centre test and
// the outline-edge area checks.
inline bool TkHasOutlineGC(const ItemStyle& style) {
    return style.outline.has_value() && style.width != 0.0;
}

// (No namespace: the core layer is global, like CanvasItem and BBox.)

int TkLineToArea(double end1X, double end1Y, double end2X, double end2Y,
                 const double rect[4]);

double TkPolygonToPoint(const Point* poly, int numPoints,
                        double px, double py);
int TkPolygonToArea(const Point* poly, int numPoints, const double rect[4]);

int TkOvalToArea(const double oval[4], const double rect[4]);

// Port of TkGetMiterPoints (tkTrig.c).  Writes the two miter apex points of
// the joint p1-p2-p3 into `m1`/`m2` and returns true; returns false for
// joints below 11 degrees, whose miter Tk ignores (the caller treats such a
// joint as a bevel).  Coordinates are rounded with floor(x + 0.5) as in Tk.
bool TkGetMiterPoints(const Point& p1, const Point& p2, const Point& p3,
                      double width, double m1[2], double m2[2]);

// Port of AngleInRange (tkCanvArc.c): is the angle of (x,y) from the origin
// within the angular range [start, start+extent], for arc's counter-clockwise,
// y-down convention (Tk negates the angle internally)?
bool TkArcAngleInRange(double x, double y, double start, double extent);

// Ports of HorizLineToArc / VertLineToArc (tkCanvArc.c). Return 1 if the
// line segment intersects the infinitely thin *arc* section (perimeter
// only; interior areas are not checked), 0 otherwise.
int TkHorizLineToArc(double x1, double x2, double y,
                     double rx, double ry, double start, double extent);
int TkVertLineToArc(double x, double y1, double y2,
                    double rx, double ry, double start, double extent);

// Port of TkOvalToPoint (tkTrig.c). Returns 0 when the point is inside the
// oval (filled) or on/inside its outline ring (unfilled), else the distance
// from the point to the nearest outline pixel (an approximation for
// eccentric ovals, like Tk's).
double TkOvalToPoint(const double oval[4], double width, bool filled,
                     const Point& pointPtr);

// Port of TkGetButtPoints. Writes the two offset points of a line end.
// `project` extends the point along the segment direction by half the width
// (Tk's projecting cap).
void TkGetButtPoints(double fromX, double fromY, double toX, double toY,
                     double width, bool project,
                     double m1[2], double m2[2]);

// Port of TkThickPolyLineToArea. `cap` selects the cap style of the two ends
// (Flat/butt, Square/projecting, Round), `join` the interior joints.
int TkThickPolyLineToArea(const Point* points, std::size_t numPoints,
                          double width, CapStyle cap, JoinStyle join,
                          const double rect[4]);

// Port of ComputeRectOvalBbox (tkRectOval.c) shared by rectangle and oval
// items. The raw coordinates are normalized (min/max), rounded half away
// from zero, and inflated by a bloat that is 1px when the item has no
// outline (Win32 rule) or (int)((width + 1) / 2) when it has one. The
// opposite side is clamped to be at least 1px away from the raw low side,
// as Tk does (the rect is always at least 1x1).
BBox TkRectOvalBBox(double x1, double y1, double x2, double y2,
                    bool hasOutline, double width);

// Port of the outline-edge half of PolygonToArea (tkCanvPoly.c): tests the
// polygon's outline against the probe rectangle. `inside` carries over the
// filled-area classification of TkPolygonToArea (the item is either fully
// inside, fully outside, or overlapping the rectangle's caught area), and
// this function escalates/confirms it with the outline test, falling back
// to 0 on any crossing. Caps are always butt (polygons have none); every
// vertex of the closed loop is tested with a round-joint oval when
// `join == JoinStyle::Round`; the closing seam joint is treated as butt,
// exactly as Tk does.
int TkPolygonOutlineToArea(const Point* points, std::size_t numPoints,
                           double width, JoinStyle join, int inside,
                           const double rect[4]);

