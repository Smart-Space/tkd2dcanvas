// TextItem.h — canvas "text" item.
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "tkd2dcanvas/CanvasItem.h"
#include "tkd2dcanvas/Anchor.h"

struct TextBlockLayout {
    std::vector<std::string> lines = {""};
    std::vector<double> lineWidths = {0.0};
    double maxLineWidth = 0.0;
};

class TextItem : public CanvasItem {
public:
    TextItem(Id id, double x, double y, std::string text);

    BBox GetBBox() const override;
    void Translate(double dx, double dy) override;
    void Draw(Renderer& renderer) const override;
    double DistanceTo(Point p) const override;

    std::vector<double> GetCoordinates() const override;
    bool SetCoordinates(const std::vector<double>& coords) override;

    double X() const { return x_; }
    double Y() const { return y_; }
    const std::string& Text() const { return text_; }
    void SetText(std::string text) { text_ = std::move(text); }

    const std::string& FontSpec() const { return fontSpec_; }
    void SetFontSpec(std::string spec) { fontSpec_ = std::move(spec); }
    const std::string& Anchor() const { return anchor_; }
    void SetAnchor(std::string anchor) { anchor_ = std::move(anchor); }
    const std::string& Justify() const { return justify_; }
    void SetJustify(std::string justify) { justify_ = std::move(justify); }
    // Wrap length in canvas units; 0 (the default) disables word wrap.
    double WrapWidth() const { return wrapWidth_; }
    void SetWrapWidth(double width) { wrapWidth_ = width; }
    // Wrap granularity: "word" (default), "char", "none" (never wraps).
    const std::string& WrapMode() const { return wrapMode_; }
    void SetWrapMode(std::string mode) { wrapMode_ = std::move(mode); }

    struct FontStyle {
        std::string family = "TkDefaultFont";
        double size = 12.0;
        bool bold = false;
        bool italic = false;
        bool underline = false;
        bool overstrike = false;
    };
    const FontStyle& Font() const { return font_; }
    void SetFontStyle(FontStyle font) {
        font_ = std::move(font);
        lineHeight_ = TextLineHeight(font_.size);
    }

    void SetLineHeight(double height);
    double LineHeight() const { return lineHeight_; }

    void SetAngle(double angle);
    double Angle() const { return angle_; }
    double AngleSine() const { return sine_; }
    double AngleCosine() const { return cosine_; }

    void SetLayout(TextBlockLayout layout);
    const TextBlockLayout& Layout() const { return layout_; }

    double BlockWidth() const { return layout_.maxLineWidth; }
    double BlockHeight() const { return static_cast<double>(layout_.lines.size()) * LineHeight(); }

private:
    // Distance from `p` to the rectangle of line `index` (0 = no hit region).
    static double DistanceToRect(double px, double py, const BBox& rect);

    double x_, y_;
    std::string text_;
    std::string fontSpec_ = "TkDefaultFont";
    std::string anchor_ = "center";
    std::string justify_ = "left";
    double wrapWidth_ = 0.0;
    std::string wrapMode_ = "word";
    FontStyle font_;
    TextBlockLayout layout_;
    double lineHeight_ = TextLineHeight(font_.size);
    double angle_ = 0.0;
    double sine_ = 0.0;
    double cosine_ = 1.0;
};
