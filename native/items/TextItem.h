// TextItem.h — canvas "text" item.
#pragma once

#include <string>

#include "tkd2dcanvas/CanvasItem.h"

class TextItem : public CanvasItem {
public:
    TextItem(Id id, double x, double y, std::string text);

    BBox GetBBox() const override;
    void Translate(double dx, double dy) override;
    void Draw(Renderer& renderer) const override;

    double X() const { return x_; }
    double Y() const { return y_; }
    const std::string& Text() const { return text_; }
    void SetText(std::string text) { text_ = std::move(text); }

    // Styling is applied via item options (-font, -fill, -anchor, -justify,
    // -width); see CanvasOptions / CanvasItemCommand. Metrics derive from
    // the resolved Tk font at draw time.
    double FontSize() const { return fontSize_; }
    void SetFontSize(double size) { fontSize_ = size; }

private:
    double x_, y_;
    std::string text_;
    double fontSize_ = 12.0;  // updated from -font option when configured
};
