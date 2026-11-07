// CanvasView.h — view window / scroll region state (scrollregion, xview, yview).
#pragma once

#include "tkd2dcanvas/BBox.h"

class CanvasView {
public:
    struct Fraction {
        double first = 0.0;  // 0.0 .. 1.0
        double last = 1.0;
    };

    void SetScrollRegion(const BBox& region) { scrollRegion_ = region; }
    const BBox& ScrollRegion() const { return scrollRegion_; }

    void SetViewSize(int width, int height) { viewWidth_ = width; viewHeight_ = height; }
    int ViewWidth() const { return viewWidth_; }
    int ViewHeight() const { return viewHeight_; }

    void SetConfine(bool confine) { confine_ = confine; }
    bool Confine() const { return confine_; }

    // Offset in canvas units of the view's top-left corner.
    double XOffset() const { return xOffset_; }
    double YOffset() const { return yOffset_; }

    void SetOffset(double x, double y);              // clamps when confine_
    void ScrollToFraction(bool horizontal, double fraction);
    Fraction XViewFraction() const;
    Fraction YViewFraction() const;

    // One "unit" of scrolling; mirrors Tk -xscrollincrement semantics.
    void SetXScrollIncrement(double units) { xIncrement_ = units; }
    void SetYScrollIncrement(double units) { yIncrement_ = units; }
    double XScrollIncrement() const { return xIncrement_; }
    double YScrollIncrement() const { return yIncrement_; }

private:
    BBox scrollRegion_ = BBox::Empty();
    int viewWidth_ = 0;
    int viewHeight_ = 0;
    double xOffset_ = 0.0;
    double yOffset_ = 0.0;
    double xIncrement_ = 0.0;   // 0 = pixel-precise
    double yIncrement_ = 0.0;
    bool confine_ = true;
};
