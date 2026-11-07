// WindowItem.h — canvas "window" item (embeds a real Tk child widget).
// NOTE: this is the most complex item type; planned for M10, not supported
// in early releases. See docs/tk_canvas_compat_matrix.md.
#pragma once

#include <string>
#include <utility>

#include "tkd2dcanvas/CanvasItem.h"

class WindowItem : public CanvasItem {
public:
    WindowItem(Id id, double x, double y, std::string windowPath);

    BBox GetBBox() const override;
    void Translate(double dx, double dy) override;
    void Draw(Renderer& renderer) const override;

    double X() const { return x_; }
    double Y() const { return y_; }
    const std::string& WindowPath() const { return windowPath_; }

    void SetSize(int width, int height) { width_ = width; height_ = height; }

private:
    double x_, y_;
    std::string windowPath_;  // Tk path of the embedded widget (-window)
    int width_ = 0;
    int height_ = 0;
};
