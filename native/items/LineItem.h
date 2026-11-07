// LineItem.h — canvas "line" item (polyline).
#pragma once

#include <vector>
#include <utility>

#include "tkd2dcanvas/CanvasItem.h"

class LineItem : public CanvasItem {
public:
    LineItem(Id id, std::vector<Point> points);

    BBox GetBBox() const override;
    void Translate(double dx, double dy) override;
    bool HitTest(Point p, double tolerance) const override;
    void Draw(Renderer& renderer) const override;

    const std::vector<Point>& Points() const { return points_; }
    void SetPoints(std::vector<Point> points) { points_ = std::move(points); }

    std::vector<double> GetCoordinates() const override;
    bool SetCoordinates(const std::vector<double>& coords) override;

private:
    std::vector<Point> points_;
};
