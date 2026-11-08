// WindowItem.h — canvas "window" item (embeds a real Tk child widget).
// Mirrors Tk's window item (tkCanvWind.c): the item stores the positioning
// point, the -window path, the -anchor and the requested -width/-height
// (0 = the embedded window's natural size). The core layer stays free of
// Tcl/Tk/Win32; the glue layer (tk_glue/CanvasTkWindowItem.cpp) resolves the
// path, manages the embedded HWND/Tk_Window and keeps its position in sync
// with the view origin, the same way Tk's DisplayWinItem() repositions the
// child during redraws.
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "tkd2dcanvas/CanvasItem.h"

class WindowItem : public CanvasItem {
public:
    WindowItem(Id id, double x, double y, std::string windowPath = "");

    BBox GetBBox() const override;
    void Translate(double dx, double dy) override;
    void Scale(double xOrigin, double yOrigin, double xScale, double yScale) override;
    void Draw(Renderer& renderer) const override;

    std::vector<double> GetCoordinates() const override;
    bool SetCoordinates(const std::vector<double>& coords) override;

    double X() const { return x_; }
    double Y() const { return y_; }
    const std::string& WindowPath() const { return windowPath_; }
    const std::string& Anchor() const { return anchor_; }

    int RequestedWidth() const { return itemWidth_; }
    int RequestedHeight() const { return itemHeight_; }

    int NaturalWidth() const { return naturalWidth_; }
    int NaturalHeight() const { return naturalHeight_; }
    void SetNaturalSize(int width, int height);

    void SetWindowPath(const std::string& path) { windowPath_ = path; }
    void SetAnchor(const std::string& anchor) { anchor_ = anchor; }
    void SetRequestedSize(int width, int height);

    int EffectiveWidth() const;
    int EffectiveHeight() const;

private:
    double x_, y_;
    std::string windowPath_;  // Tk path of the embedded widget (-window); "" = none
    std::string anchor_ = "center";  // Tk default
    int itemWidth_ = 0;      // -width  (pixels, 0 = natural)
    int itemHeight_ = 0;     // -height (pixels, 0 = natural)
    int naturalWidth_ = 1;   // embedded window's requested size
    int naturalHeight_ = 1;
};
