// BitmapItem.h — canvas "bitmap" item (XBM-style bitmap from -bitmap option).
#pragma once

#include <string>
#include <utility>

#include "tkd2dcanvas/CanvasItem.h"

class BitmapItem : public CanvasItem {
public:
    BitmapItem(Id id, double x, double y, std::string bitmapName);

    BBox GetBBox() const override;
    void Translate(double dx, double dy) override;
    void Draw(Renderer& renderer) const override;

    double X() const { return x_; }
    double Y() const { return y_; }
    const std::string& BitmapName() const { return bitmapName_; }

    // Resolved from the bitmap resource at draw time (Tk builtin bitmaps
    // or @file.xbm). Until then, report a nominal size.
    void SetResolvedSize(int width, int height) { width_ = width; height_ = height; }

private:
    double x_, y_;
    std::string bitmapName_;
    int width_ = 0;
    int height_ = 0;
};
