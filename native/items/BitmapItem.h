// BitmapItem.h — canvas "bitmap" item (XBM-style 1-bit bitmaps from the
// -bitmap option: Tk builtin names like "info", or "@file.xbm").
//
// The 1-bit source is expanded to RGBA at configure time using the
// -foreground (set bits) and -background (clear bits, transparent by
// default) colors, then drawn through the same cached-bitmap path as photo
// items. Reconfiguring -foreground / -background re-bakes the pixels.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "tkd2dcanvas/CanvasItem.h"
#include "tkd2dcanvas/Color.h"
#include "tkd2dcanvas/Renderer.h"   // ImageRef
#include "tkd2dcanvas/Anchor.h"

class BitmapItem : public CanvasItem {
public:
    BitmapItem(Id id, double x, double y, std::string bitmapName);

    BBox GetBBox() const override;
    void Translate(double dx, double dy) override;
    void Draw(Renderer& renderer) const override;

    std::vector<double> GetCoordinates() const override;
    bool SetCoordinates(const std::vector<double>& coords) override;

    double X() const { return x_; }
    double Y() const { return y_; }
    const std::string& BitmapName() const { return bitmapName_; }
    void SetBitmapName(std::string name) { bitmapName_ = std::move(name); }

    const std::string& Anchor() const { return anchor_; }
    void SetAnchor(std::string anchor) { anchor_ = std::move(anchor); }

    bool HasForeground() const { return foreground_.has_value(); }
    Color Foreground() const { return foreground_.value_or(Color{0, 0, 0}); }
    bool HasBackground() const { return background_.has_value(); }
    Color Background() const { return background_.value_or(Color{0, 0, 0}); }

    // Replaces the 1-bit source (row-major, (width + 7) / 8 bytes per row,
    // LSB first — the XBM layout). Re-bakes the RGBA pixels.
    void SetBitmapBits(std::vector<std::uint8_t> bits, int width, int height);
    // Re-bakes the RGBA pixels with new paint (bumps the cache generation).
    void SetBackground(std::optional<Color> color);
    void SetForeground(std::optional<Color> color);

    int PixelWidth() const { return width_; }
    int PixelHeight() const { return height_; }

private:
    void BakePixels();

    double x_, y_;
    std::string bitmapName_;
    std::string anchor_ = "center";     // Tk default
    std::optional<Color> foreground_ = Color{0, 0, 0};  // Tk default: black
    std::optional<Color> background_;                  // Tk default: transparent
    std::vector<std::uint8_t> bits_;
    int width_ = 0;
    int height_ = 0;
    std::vector<std::uint8_t> pixels_;  // RGBA, straight alpha
    unsigned generation_ = 0;
};
