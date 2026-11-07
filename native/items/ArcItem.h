// ArcItem.h — canvas "arc" item (ellipse sector/pie/chord).
#pragma once

#include "tkd2dcanvas/CanvasItem.h"

class ArcItem : public CanvasItem {
public:
    // Tk: -start degrees (counter-clockwise from 3 o'clock), -extent degrees.
    ArcItem(Id id, double x1, double y1, double x2, double y2, double startDeg, double extentDeg);

    BBox GetBBox() const override;
    void Translate(double dx, double dy) override;
    void Draw(Renderer& renderer) const override;

    double Start() const { return start_; }
    double Extent() const { return extent_; }
    void SetAngles(double startDeg, double extentDeg) { start_ = startDeg; extent_ = extentDeg; }
    std::vector<double> Coordinates() const { return {x1_, y1_, x2_, y2_}; }

private:
    double x1_, y1_, x2_, y2_;
    double start_, extent_;
};
