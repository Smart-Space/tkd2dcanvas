// ImageItem.h — canvas "image" item backed by a Tk photo image.
//
// Pixels are copied out of the Tk photo (TkPhotoToRgba) at configure time
// and stored on the item, so Draw() needs nothing but the borrowed RGBA
// buffer; the renderer converts (and caches) the backing bitmap itself.
// Changes to the photo made *after* configuration are not reflected until
// the -image option is configured again (Tk pushes change notifications to
// registered image items; that live tracking is not implemented yet).
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "tkd2dcanvas/CanvasItem.h"
#include "tkd2dcanvas/Renderer.h"   // ImageRef
#include "tkd2dcanvas/Anchor.h"

class ImageItem : public CanvasItem {
public:
    ImageItem(Id id, double x, double y, std::string imageName);

    BBox GetBBox() const override;
    void Translate(double dx, double dy) override;
    void Draw(Renderer& renderer, Path& path) const override;

    std::vector<double> GetCoordinates() const override;
    bool SetCoordinates(const std::vector<double>& coords) override;

    double X() const { return x_; }
    double Y() const { return y_; }
    const std::string& ImageName() const { return imageName_; }
    void SetImageName(std::string name) { imageName_ = std::move(name); }

    const std::string& Anchor() const { return anchor_; }
    void SetAnchor(std::string anchor) { anchor_ = std::move(anchor); }

    int PixelWidth() const { return width_; }
    int PixelHeight() const { return height_; }
    // Replaces the cached pixels (RGBA, straight alpha, tightly packed) and
    // bumps the cache generation the renderer keys its bitmap cache on.
    void SetPixels(std::vector<std::uint8_t> rgba, int width, int height);
    void ClearPixels();

private:
    double x_, y_;
    std::string imageName_;             // Tk image name (-image option)
    std::string anchor_ = "center";     // Tk default
    std::vector<std::uint8_t> pixels_;  // RGBA, straight alpha, tightly packed
    int width_ = 0;
    int height_ = 0;
    unsigned generation_ = 0;
};
