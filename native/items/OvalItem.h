// OvalItem.h — canvas "oval" item (ellipse fitting the given bbox).
#pragma once

#include "tkd2dcanvas/CanvasItem.h"

class OvalItem : public CanvasItem {
public:
    OvalItem(Id id, double x1, double y1, double x2, double y2);

    BBox GetBBox() const override;
    void Translate(double dx, double dy) override;
    bool HitTest(Point p, double tolerance) const override;
    void Draw(Renderer& renderer) const override;

    std::vector<double> Coordinates() const { return {x1_, y1_, x2_, y2_}; }
    void SetCoordinates(double x1, double y1, double x2, double y2);

private:
    double x1_, y1_, x2_, y2_;
};
