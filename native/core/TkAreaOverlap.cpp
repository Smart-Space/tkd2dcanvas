#include "tkd2dcanvas/TkAreaOverlap.h"

#include <algorithm>
#include <cmath>

bool TkArcAngleInRange(double x, double y, double start, double extent) {
    if ((x == 0.0) && (y == 0.0)) {
        return true;
    }
    constexpr double kPi = 3.14159265358979323846;
    double diff = -std::atan2(y, x);
    diff = diff * (180.0 / kPi) - start;
    while (diff > 360.0) diff -= 360.0;
    while (diff < 0.0) diff += 360.0;
    if (extent >= 0) return diff <= extent;
    return (diff - 360.0) >= extent;
}

int TkHorizLineToArc(double x1, double x2, double y,
                     double rx, double ry, double start, double extent) {
    const double ty = y / ry;
    const double tmp = 1.0 - ty * ty;
    if (tmp < 0.0) return 0;
    const double tx = std::sqrt(tmp);
    const double x = tx * rx;
    if ((x >= x1) && (x <= x2) && TkArcAngleInRange(tx, ty, start, extent)) {
        return 1;
    }
    if ((-x >= x1) && (-x <= x2) && TkArcAngleInRange(-tx, ty, start, extent)) {
        return 1;
    }
    return 0;
}

int TkVertLineToArc(double x, double y1, double y2,
                    double rx, double ry, double start, double extent) {
    const double tx = x / rx;
    const double tmp = 1.0 - tx * tx;
    if (tmp < 0.0) return 0;
    const double ty = std::sqrt(tmp);
    const double y = ty * ry;
    if ((y > y1) && (y < y2) && TkArcAngleInRange(tx, ty, start, extent)) {
        return 1;
    }
    if ((-y > y1) && (-y < y2) && TkArcAngleInRange(tx, -ty, start, extent)) {
        return 1;
    }
    return 0;
}

double TkOvalToPoint(const double oval[4], double width, bool filled,
                     const Point& pointPtr) {
    const double xDelta = pointPtr.x - (oval[0] + oval[2]) / 2.0;
    const double yDelta = pointPtr.y - (oval[1] + oval[3]) / 2.0;
    const double distToCenter = std::hypot(xDelta, yDelta);
    const double scaledDistance = std::hypot(xDelta / ((oval[2] + width - oval[0]) / 2.0),
                                             yDelta / ((oval[3] + width - oval[1]) / 2.0));

    if (scaledDistance > 1.0) {
        return (distToCenter / scaledDistance) * (scaledDistance - 1.0);
    }
    if (filled) {
        return 0.0;
    }
    double distToOutline;
    if (scaledDistance > 1e-10) {
        distToOutline = (distToCenter / scaledDistance) * (1.0 - scaledDistance) - width;
    } else {
        const double xDiam = oval[2] - oval[0];
        const double yDiam = oval[3] - oval[1];
        distToOutline = ((xDiam < yDiam) ? (xDiam - width) : (yDiam - width)) / 2.0;
    }
    if (distToOutline < 0.0) {
        return 0.0;
    }
    return distToOutline;
}

bool TkGetMiterPoints(const Point& p1, const Point& p2, const Point& p3,
                      double width, double m1[2], double m2[2]) {
    constexpr double kPi = 3.14159265358979323846;
    constexpr double kElevenDegrees = (11.0 * 2.0 * kPi) / 360.0;

    const double p1x = std::floor(p1.x + 0.5);
    const double p1y = std::floor(p1.y + 0.5);
    const double p2x = std::floor(p2.x + 0.5);
    const double p2y = std::floor(p2.y + 0.5);
    const double p3x = std::floor(p3.x + 0.5);
    const double p3y = std::floor(p3.y + 0.5);

    double theta1;
    if (p2y == p1y) {
        theta1 = (p2x < p1x) ? 0.0 : kPi;
    } else if (p2x == p1x) {
        theta1 = (p2y < p1y) ? kPi / 2.0 : -kPi / 2.0;
    } else {
        theta1 = std::atan2(p1y - p2y, p1x - p2x);
    }

    double theta2;
    if (p3y == p2y) {
        theta2 = (p3x > p2x) ? 0.0 : kPi;
    } else if (p3x == p2x) {
        theta2 = (p3y > p2y) ? kPi / 2.0 : -kPi / 2.0;
    } else {
        theta2 = std::atan2(p3y - p2y, p3x - p2x);
    }

    double theta = theta1 - theta2;
    if (theta > kPi) {
        theta -= 2.0 * kPi;
    } else if (theta < -kPi) {
        theta += 2.0 * kPi;
    }

    if ((theta < kElevenDegrees) && (theta > -kElevenDegrees)) {
        return false;
    }

    double dist = 0.5 * width / std::sin(0.5 * theta);
    if (dist < 0.0) {
        dist = -dist;
    }

    // Make sure the bisector points away from the turn (as in Tk).
    double theta3 = (theta1 + theta2) / 2.0;
    if (std::sin(theta3 - (theta1 + kPi)) < 0.0) {
        theta3 += kPi;
    }

    const double deltaX = dist * std::cos(theta3);
    const double deltaY = dist * std::sin(theta3);
    m1[0] = p2x + deltaX;
    m1[1] = p2y + deltaY;
    m2[0] = p2x - deltaX;
    m2[1] = p2y - deltaY;
    return true;
}

int TkLineToArea(double end1X, double end1Y, double end2X, double end2Y,
                 const double rect[4]) {
    const bool inside1 = (end1X >= rect[0]) && (end1X <= rect[2])
        && (end1Y >= rect[1]) && (end1Y <= rect[3]);
    const bool inside2 = (end2X >= rect[0]) && (end2X <= rect[2])
        && (end2Y >= rect[1]) && (end2Y <= rect[3]);
    if (inside1 != inside2) {
        return 0;
    }
    if (inside1 & inside2) {
        return 1;
    }

    // Both points are outside; still check the line against the edges.
    if (end1X == end2X) {
        // Vertical line.
        const bool crosses = ((end1Y >= rect[1]) != (end2Y >= rect[1]))
            && (end1X >= rect[0]) && (end1X <= rect[2]);
        if (crosses) {
            return 0;
        }
    } else if (end1Y == end2Y) {
        // Horizontal line.
        const bool crosses = ((end1X >= rect[0]) != (end2X >= rect[0]))
            && (end1Y >= rect[1]) && (end1Y <= rect[3]);
        if (crosses) {
            return 0;
        }
    } else {
        const double m = (end2Y - end1Y) / (end2X - end1X);
        double low, high;
        if (end1X < end2X) {
            low = end1X;
            high = end2X;
        } else {
            low = end2X;
            high = end1X;
        }

        // Left edge.
        double y = end1Y + (rect[0] - end1X) * m;
        if ((rect[0] >= low) && (rect[0] <= high)
            && (y >= rect[1]) && (y <= rect[3])) {
            return 0;
        }

        // Right edge.
        y += (rect[2] - rect[0]) * m;
        if ((y >= rect[1]) && (y <= rect[3])
            && (rect[2] >= low) && (rect[2] <= high)) {
            return 0;
        }

        // Bottom edge.
        if (end1Y < end2Y) {
            low = end1Y;
            high = end2Y;
        } else {
            low = end2Y;
            high = end1Y;
        }
        double x = end1X + (rect[1] - end1Y) / m;
        if ((x >= rect[0]) && (x <= rect[2])
            && (rect[1] >= low) && (rect[1] <= high)) {
            return 0;
        }

        // Top edge.
        x += (rect[3] - rect[1]) / m;
        if ((x >= rect[0]) && (x <= rect[2])
            && (rect[3] >= low) && (rect[3] <= high)) {
            return 0;
        }
    }
    return -1;
}

double TkPolygonToPoint(const Point* poly, int numPoints,
                        double px, double py) {
    double bestDist = 1.0e36;
    int intersections = 0;

    for (int count = numPoints; count > 1; --count, ++poly) {
        const Point& a = poly[0];
        const Point& b = poly[1];
        double x, y;

        if (b.x == a.x) {
            // Vertical edge.
            x = a.x;
            if (a.y >= b.y) {
                y = std::min(a.y, py);
                y = std::max(y, b.y);
            } else {
                y = std::min(b.y, py);
                y = std::max(y, a.y);
            }
        } else if (b.y == a.y) {
            // Horizontal edge.
            y = a.y;
            if (a.x >= b.x) {
                x = std::min(a.x, px);
                x = std::max(x, b.x);
                if ((py < y) && (px < a.x) && (px >= b.x)) {
                    ++intersections;
                }
            } else {
                x = std::min(b.x, px);
                x = std::max(x, a.x);
                if ((py < y) && (px < b.x) && (px >= a.x)) {
                    ++intersections;
                }
            }
        } else {
            // Neither vertical nor horizontal.
            const double m1 = (b.y - a.y) / (b.x - a.x);
            const double b1 = a.y - m1 * a.x;
            const double m2 = -1.0 / m1;
            const double b2 = py - m2 * px;
            x = (b2 - b1) / (m1 - m2);
            y = m1 * x + b1;
            if (a.x > b.x) {
                if (x > a.x) {
                    x = a.x;
                    y = a.y;
                } else if (x < b.x) {
                    x = b.x;
                    y = b.y;
                }
            } else {
                if (x > b.x) {
                    x = b.x;
                    y = b.y;
                } else if (x < a.x) {
                    x = a.x;
                    y = a.y;
                }
            }
            const bool lower = (m1 * px + b1) > py;
            if (lower && (px >= std::min(a.x, b.x)) && (px < std::max(a.x, b.x))) {
                ++intersections;
            }
        }

        const double dist = std::hypot(px - x, py - y);
        if (dist < bestDist) {
            bestDist = dist;
        }
    }

    if ((intersections & 0x1) != 0) {
        return 0.0;
    }
    return bestDist;
}

int TkPolygonToArea(const Point* poly, int numPoints, const double rect[4]) {
    const Point* originalPoly = poly;
    // Test the edges against the rectangle; quit early on the first crossing.
    int state = TkLineToArea(poly[0].x, poly[0].y, poly[1].x, poly[1].y, rect);
    if (state == 0) {
        return 0;
    }
    for (int count = numPoints - 1; count >= 2; --count, ++poly) {
        if (TkLineToArea(poly[1].x, poly[1].y, poly[2].x, poly[2].y, rect)
            != state) {
            return 0;
        }
    }

    if (state == 1) {
        return 1;
    }
    // All edges outside: the rectangle could still be enclosed by the polygon.
    if (TkPolygonToPoint(originalPoly, numPoints, rect[0], rect[1]) == 0.0) {
        return 0;
    }
    return -1;
}

int TkOvalToArea(const double oval[4], const double rect[4]) {
    // Entirely inside or entirely outside?
    if ((rect[0] <= oval[0]) && (rect[2] >= oval[2])
        && (rect[1] <= oval[1]) && (rect[3] >= oval[3])) {
        return 1;
    }
    if ((rect[2] < oval[0]) || (rect[0] > oval[2])
        || (rect[3] < oval[1]) || (rect[1] > oval[3])) {
        return -1;
    }

    const double centerX = (oval[0] + oval[2]) / 2.0;
    const double centerY = (oval[1] + oval[3]) / 2.0;
    const double radX = (oval[2] - oval[0]) / 2.0;
    const double radY = (oval[3] - oval[1]) / 2.0;

    double deltaY = rect[1] - centerY;
    if (deltaY < 0.0) {
        deltaY = centerY - rect[3];
        if (deltaY < 0.0) {
            deltaY = 0.0;
        }
    }
    deltaY = deltaY / radY;
    deltaY *= deltaY;

    // Left side.
    double deltaX = (rect[0] - centerX) / radX;
    deltaX *= deltaX;
    if ((deltaX + deltaY) <= 1.0) {
        return 0;
    }

    // Right side.
    deltaX = (rect[2] - centerX) / radX;
    deltaX *= deltaX;
    if ((deltaX + deltaY) <= 1.0) {
        return 0;
    }

    deltaX = rect[0] - centerX;
    if (deltaX < 0.0) {
        deltaX = centerX - rect[2];
        if (deltaX < 0.0) {
            deltaX = 0.0;
        }
    }
    deltaX = deltaX / radX;
    deltaX *= deltaX;

    // Bottom side.
    deltaY = (rect[1] - centerY) / radY;
    deltaY *= deltaY;
    if ((deltaX + deltaY) < 1.0) {
        return 0;
    }

    // Top side.
    deltaY = (rect[3] - centerY) / radY;
    deltaY *= deltaY;
    if ((deltaX + deltaY) < 1.0) {
        return 0;
    }

    return -1;
}

void TkGetButtPoints(double fromX, double fromY, double toX, double toY,
                     double width, bool project,
                     double m1[2], double m2[2]) {
    width *= 0.5;
    const double length = std::hypot(toX - fromX, toY - fromY);
    if (length == 0.0) {
        m1[0] = m2[0] = toX;
        m1[1] = m2[1] = toY;
        return;
    }
    const double deltaX = -width * (toY - fromY) / length;
    const double deltaY = width * (toX - fromX) / length;
    m1[0] = toX + deltaX;
    m2[0] = toX - deltaX;
    m1[1] = toY + deltaY;
    m2[1] = toY - deltaY;
    if (project) {
        m1[0] += deltaY;
        m2[0] += deltaY;
        m1[1] -= deltaX;
        m2[1] -= deltaX;
    }
}

int TkThickPolyLineToArea(const Point* points, std::size_t numPoints,
                          double width, CapStyle cap, JoinStyle join,
                          const double rect[4]) {
    const double radius = width / 2.0;
    int inside = (points[0].x >= rect[0] && points[0].x <= rect[2]
                  && points[0].y >= rect[1] && points[0].y <= rect[3])
        ? 1 : -1;

    // Track whether the last miter had to be treated as a bevel; the wedge
    // between that joint's butt points is then tested as its own polygon.
    bool changedMiterToBevel = false;

    for (std::size_t seg = 0; seg + 1 < numPoints; ++seg) {
        const Point& cur = points[seg];
        const Point& next = points[seg + 1];
        const bool isFirst = (seg == 0);
        const bool isLast = (seg + 1 == numPoints - 1);

        // Round caps/joints around the leading point of the edge.
        if ((cap == CapStyle::Round && isFirst)
            || (join == JoinStyle::Round && !isFirst)) {
            const double oval[4] = {cur.x - radius, cur.y - radius,
                                    cur.x + radius, cur.y + radius};
            if (TkOvalToArea(oval, rect) != inside) {
                return 0;
            }
        }

        // Polygon for this edge: butt points at its start (or the miter
        // apexes inherited from the previous edge), then butt/miter points
        // at its end.
        double poly[10];
        if (isFirst) {
            TkGetButtPoints(next.x, next.y, cur.x, cur.y, width,
                            /*project=*/cap == CapStyle::Square,
                            poly, poly + 2);
        } else if (join == JoinStyle::Miter && !changedMiterToBevel) {
            poly[0] = poly[6];
            poly[1] = poly[7];
            poly[2] = poly[4];
            poly[3] = poly[5];
        } else {
            TkGetButtPoints(next.x, next.y, cur.x, cur.y, width,
                            /*project=*/false,
                            poly, poly + 2);
            if ((join == JoinStyle::Bevel) || changedMiterToBevel) {
                // Wedge filling the (previous) beveled joint.
                const Point wedge[5] = {
                    {poly[0], poly[1]}, {poly[2], poly[3]},
                    {poly[4], poly[5]}, {poly[6], poly[7]},
                    {poly[0], poly[1]},
                };
                if (TkPolygonToArea(wedge, 5, rect) != inside) {
                    return 0;
                }
                changedMiterToBevel = false;
            }
        }

        if (isLast) {
            TkGetButtPoints(cur.x, cur.y, next.x, next.y, width,
                            /*project=*/cap == CapStyle::Square,
                            poly + 4, poly + 6);
        } else if (join == JoinStyle::Miter) {
            if (!TkGetMiterPoints(points[seg], points[seg + 1],
                                  points[seg + 2], width,
                                  poly + 4, poly + 6)) {
                changedMiterToBevel = true;
                TkGetButtPoints(cur.x, cur.y, next.x, next.y, width,
                                /*project=*/false,
                                poly + 4, poly + 6);
            }
        } else {
            TkGetButtPoints(cur.x, cur.y, next.x, next.y, width,
                            /*project=*/false,
                            poly + 4, poly + 6);
        }

        const Point edge[5] = {
            {poly[0], poly[1]}, {poly[2], poly[3]},
            {poly[4], poly[5]}, {poly[6], poly[7]},
            {poly[0], poly[1]},
        };
        if (TkPolygonToArea(edge, 5, rect) != inside) {
            return 0;
        }
    }

    // Round cap around the final point of the line.
    if (cap == CapStyle::Round) {
        const Point& last = points[numPoints - 1];
        const double oval[4] = {last.x - radius, last.y - radius,
                                last.x + radius, last.y + radius};
        if (TkOvalToArea(oval, rect) != inside) {
            return 0;
        }
    }

    return inside;
}

BBox TkRectOvalBBox(double x1, double y1, double x2, double y2,
                    bool hasOutline, double width) {
    // Tk keeps the *raw* coordinates (normalized to min/max) in the item
    // header; the integer box is derived from them, with the low side
    // rounded and the high side clamped to be at least 1 unit above the
    // raw low side (the rect is drawn at least 1x1).
    const double loX = (x1 < x2) ? x1 : x2;
    const double hiX = (x1 < x2) ? x2 : x1;
    const double loY = (y1 < y2) ? y1 : y2;
    const double hiY = (y1 < y2) ? y2 : y1;

    // Win32 rule: no outline -> a 1px bloat regardless of width; otherwise
    // (int)((width + 1) / 2).
    const int bloat = hasOutline ? static_cast<int>((width + 1.0) / 2.0) : 1;

    const auto roundNearest = [](double v) {
        // (v >= 0) ? (int)(v + .5) : (int)(v - .5) -- rounds half away
        // from zero (a bare C cast truncates toward zero).
        return static_cast<int>((v >= 0.0) ? v + 0.5 : v - 0.5);
    };

    const int ix1 = roundNearest(loX) - bloat;
    const int iy1 = roundNearest(loY) - bloat;
    double rx2 = hiX;
    if (rx2 < loX + 1.0) {
        rx2 = loX + 1.0;
    }
    double ry2 = hiY;
    if (ry2 < loY + 1.0) {
        ry2 = loY + 1.0;
    }
    const int ix2 = roundNearest(rx2) + bloat;
    const int iy2 = roundNearest(ry2) + bloat;
    return BBox{double(ix1), double(iy1), double(ix2), double(iy2)};
}

int TkPolygonOutlineToArea(const Point* points, std::size_t numPoints,
                           double width, JoinStyle join, int inside,
                           const double rect[4]) {
    const double radius = width / 2.0;
    bool changedMiterToBevel = false;

    for (std::size_t seg = 0; seg + 1 < numPoints; ++seg) {
        const Point& cur = points[seg];
        const Point& next = points[seg + 1];
        const bool isFirst = (seg == 0);
        const bool isLast = (seg + 1 == numPoints - 1);

        // Round joins: a circular region is tested around EVERY vertex of
        // the closed loop (unlike a line, where the very first point is a
        // cap, not a joint).
        if (join == JoinStyle::Round) {
            const double oval[4] = {cur.x - radius, cur.y - radius,
                                    cur.x + radius, cur.y + radius};
            if (TkOvalToArea(oval, rect) != inside) {
                return 0;
            }
        }

        // Polygonal shape of this edge: butt points at its start (or the
        // miter apexes inherited from the previous edge), then miter/butt
        // points at its end. Caps are always butt.
        double poly[10];
        if (isFirst) {
            TkGetButtPoints(next.x, next.y, cur.x, cur.y, width,
                            /*project=*/false,
                            poly, poly + 2);
        } else if (join == JoinStyle::Miter && !changedMiterToBevel) {
            poly[0] = poly[6];
            poly[1] = poly[7];
            poly[2] = poly[4];
            poly[3] = poly[5];
        } else {
            TkGetButtPoints(next.x, next.y, cur.x, cur.y, width,
                            /*project=*/false,
                            poly, poly + 2);
            if ((join == JoinStyle::Bevel) || changedMiterToBevel) {
                const Point wedge[5] = {
                    {poly[0], poly[1]}, {poly[2], poly[3]},
                    {poly[4], poly[5]}, {poly[6], poly[7]},
                    {poly[0], poly[1]},
                };
                if (TkPolygonToArea(wedge, 5, rect) != inside) {
                    return 0;
                }
                changedMiterToBevel = false;
            }
        }

        if (isLast) {
            TkGetButtPoints(cur.x, cur.y, next.x, next.y, width,
                            /*project=*/false,
                            poly + 4, poly + 6);
        } else if (join == JoinStyle::Miter) {
            if (!TkGetMiterPoints(points[seg], points[seg + 1],
                                  points[seg + 2], width,
                                  poly + 4, poly + 6)) {
                changedMiterToBevel = true;
                TkGetButtPoints(cur.x, cur.y, next.x, next.y, width,
                                /*project=*/false,
                                poly + 4, poly + 6);
            }
        } else {
            TkGetButtPoints(cur.x, cur.y, next.x, next.y, width,
                            /*project=*/false,
                            poly + 4, poly + 6);
        }

        const Point edge[5] = {
            {poly[0], poly[1]}, {poly[2], poly[3]},
            {poly[4], poly[5]}, {poly[6], poly[7]},
            {poly[0], poly[1]},
        };
        if (TkPolygonToArea(edge, 5, rect) != inside) {
            return 0;
        }
    }

    return inside;
}

