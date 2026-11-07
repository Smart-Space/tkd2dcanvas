// PolygonItem.h — canvas "polygon" item (closed polyline).
#pragma once

#include <vector>
#include <utility>

#include "tkd2dcanvas/CanvasItem.h"

class PolygonItem : public CanvasItem {
public:
    PolygonItem(Id id, std::vector<Point> points);

    BBox GetBBox() const override;
    void Translate(double dx, double dy) override;
    bool HitTest(Point p, double tolerance) const override;
    void Draw(Renderer& renderer) const override;

    const std::vector<Point>& Points() const { return points_; }
    void SetPoints(std::vector<Point> points) { points_ = std::move(points); }

private:
    std::vector<Point> points_;
};
