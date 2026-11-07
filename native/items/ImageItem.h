// ImageItem.h — canvas "image" item backed by a Tk photo/bitmap image.
#pragma once

#include <string>
#include <utility>

#include "tkd2dcanvas/CanvasItem.h"
#include "tkd2dcanvas/Renderer.h"   // ImageRef

class ImageItem : public CanvasItem {
public:
    ImageItem(Id id, double x, double y, std::string imageName);

    BBox GetBBox() const override;
    void Translate(double dx, double dy) override;
    void Draw(Renderer& renderer) const override;

    double X() const { return x_; }
    double Y() const { return y_; }
    const std::string& ImageName() const { return imageName_; }
    void SetImageName(std::string name) { imageName_ = std::move(name); }

    // Resolved at draw time through CanvasTkImage (Tk image -> D2D bitmap).
    void SetResolvedSize(int width, int height) { width_ = width; height_ = height; }

private:
    double x_, y_;
    std::string imageName_;          // Tk image name (-image option)
    int width_ = 0;
    int height_ = 0;
};
