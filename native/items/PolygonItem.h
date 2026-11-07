// PolygonItem.h — canvas "polygon" item (closed polyline).
#pragma once

#include <vector>
#include <utility>

#include "tkd2dcanvas/CanvasItem.h"
#include "tkd2dcanvas/SmoothCurve.h"

class PolygonItem : public CanvasItem {
public:
    PolygonItem(Id id, std::vector<Point> points);

    const SmoothOptions& Smooth() const { return smooth_; }
    void SetSmooth(const SmoothOptions& smooth) { smooth_ = smooth; }

    // Tk keeps a duplicated closing vertex in coordPtr when the polygon is
    // not explicitly closed; the spline generator sees that duplicate.
    // Returns the points of the polygon as Tk would pass them to
    // TkMakeBezierCurve (closing vertex appended when missing).
    std::vector<Point> TkPoints() const;

    // Points actually painted (Tk DisplayPolygon): the flattened -smooth
    // curve when -smooth is set and Tk would use the smoothed path,
    // otherwise the raw vertices.
    std::vector<Point> DisplayPoints() const;

    BBox GetBBox() const override;
    void Translate(double dx, double dy) override;
    double DistanceTo(Point p) const override;
    void Draw(Renderer& renderer) const override;

    const std::vector<Point>& Points() const { return points_; }
    void SetPoints(std::vector<Point> points) { points_ = std::move(points); }

    std::vector<double> GetCoordinates() const override;
    bool SetCoordinates(const std::vector<double>& coords) override;

private:
    std::vector<Point> points_;
    SmoothOptions smooth_;
};
