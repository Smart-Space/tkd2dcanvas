// RectItem.h — canvas "rectangle" item.
#pragma once

#include "tkd2dcanvas/CanvasItem.h"

class RectItem : public CanvasItem {
public:
    RectItem(Id id, double x1, double y1, double x2, double y2);

    BBox GetBBox() const override;
    void Translate(double dx, double dy) override;
    int AreaOverlap(const double rect[4]) const override;
    double DistanceTo(Point p) const override;
    void Draw(Renderer& renderer) const override;

    std::vector<double> GetCoordinates() const override { return {x1_, y1_, x2_, y2_}; }
    bool SetCoordinates(const std::vector<double>& coords) override;

    std::vector<double> Coordinates() const { return GetCoordinates(); }
    void SetCoordinates(double x1, double y1, double x2, double y2);

private:
    double x1_, y1_, x2_, y2_;
};
