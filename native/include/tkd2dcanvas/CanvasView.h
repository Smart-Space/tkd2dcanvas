// CanvasView.h — view window / scroll region state (scrollregion, xview, yview).
//
// The semantics follow Tk's canvas (tkCanvas.c: CanvasSetOrigin,
// ScrollFractions), translated to doubles instead of pixel ints:
//   - the scroll region is the "total range" in canvas units
//     (-scrollregion, left top right bottom);
//   - the view offset is the canvas coordinate at the top-left edge of the
//     window;
//   - -confine keeps the view from sticking out of the region (but never
//     pulls the view so far that the *other* side sticks out);
//   - -xscrollincrement/-yscrollincrement force the offset to a multiple of
//     the increment and make the increment the "units" step for scrolling;
//   - fractions (xview/yview, -?scrollcommand) always refer to the region.
// The border/highlight "inset" of Tk is ignored: this widget reserves no
// inset (borderwidth/highlightthickness are accepted but not drawn, so the
// visible area is the full window).
#pragma once

#include "tkd2dcanvas/BBox.h"

class CanvasView {
public:
    struct Fraction {
        double first = 0.0;  // 0.0 .. 1.0
        double last = 1.0;   // 0.0 .. 1.0
    };

    // Stores the region and re-applies rounding/confine on the current
    // origin (Tk: configuring -scrollregion re-runs CanvasSetOrigin).
    void SetScrollRegion(const BBox& region);
    const BBox& ScrollRegion() const { return scrollRegion_; }

    // Stores the window size and re-applies rounding/confine (Tk: a
    // ConfigureNotify recenters a confined canvas whose region is smaller
    // than the window).
    void SetViewSize(int width, int height);
    int ViewWidth() const { return viewWidth_; }
    int ViewHeight() const { return viewHeight_; }

    void SetConfine(bool confine) { confine_ = confine; }
    bool Confine() const { return confine_; }

    // Offset in canvas units of the view's top-left corner.
    double XOffset() const { return xOffset_; }
    double YOffset() const { return yOffset_; }

    // Rounds to -?scrollincrement (when > 0) and confines to the scroll
    // region (when -confine and a region is set), like Tk's CanvasSetOrigin.
    void SetOrigin(double x, double y);
    // moveto <fraction> — Tk maps the fraction onto the full region width
    // (not onto the scrollable span); confine pulls out-of-range values
    // back. Without a region the origin resets to 0 (Tk computes
    // scrollX1 + fraction*0).
    void MoveTo(bool horizontal, double fraction);
    // scroll <n> units — the increment when -?scrollincrement > 0, else
    // one tenth of the visible span (Tk semantics).
    void ScrollUnits(bool horizontal, int count);
    // scroll <n> pages — nine tenths of the visible span (Tk semantics).
    void ScrollPages(bool horizontal, int count);

    Fraction XViewFraction() const;
    Fraction YViewFraction() const;

    // One "unit" of scrolling; mirrors Tk -?scrollincrement semantics.
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
    double xIncrement_ = 0.0;   // 0 = no constraint
    double yIncrement_ = 0.0;   // 0 = no constraint
    bool confine_ = true;

    static double RoundToIncrement(double value, double increment);
    void ConfineOrigin(double& x, double& y) const;
};
