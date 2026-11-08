// ArcItem.h — canvas "arc" item (ellipse sector/pie/chord).
#pragma once

#include <vector>

#include "tkd2dcanvas/CanvasItem.h"

// Tk's -style option for arc items.
enum class ArcStyle {
    PieSlice,  // arc + the two radial lines back to the center
    Chord,     // arc + a straight chord between the endpoints
    Arc,       // only the arc line
};

class ArcItem : public CanvasItem {
public:
    // Tk: -start degrees (counter-clockwise from 3 o'clock), -extent degrees.
    // The arc is inscribed in the x1 y1 x2 y2 bounding box. Start defaults to
    // 0 and extent to 90 (Tk defaults); style defaults to pieslice. Tk
    // normalizes -start/-extent at configuration time (start into [0,360),
    // extent into (-360,360]).
    ArcItem(Id id, double x1, double y1, double x2, double y2, double startDeg = 0.0, double extentDeg = 90.0);

    BBox GetBBox() const override;
    BBox DamageBBox() const override;
    void Translate(double dx, double dy) override;
    double DistanceTo(Point p) const override;
    int AreaOverlap(const double rect[4]) const override;
    void Draw(Renderer& renderer) const override;

    double Start() const { return start_; }
    double Extent() const { return extent_; }
    void SetAngles(double startDeg, double extentDeg);

    ArcStyle GetStyle() const { return style_; }
    void SetStyle(ArcStyle style) { style_ = style; }

    // "pieslice" / "chord" / "arc" (case-insensitive). False on unknown names.
    static bool StyleFromString(const std::string& text, ArcStyle& out);
    static const char* StyleName(ArcStyle style);

    std::vector<double> GetCoordinates() const override { return {x1_, y1_, x2_, y2_}; }
    bool SetCoordinates(const std::vector<double>& coords) override;

    std::vector<double> Coordinates() const { return GetCoordinates(); }

private:
    double x1_, y1_, x2_, y2_;
    double start_, extent_;
    ArcStyle style_ = ArcStyle::PieSlice;

    std::vector<Point> SampleArc() const;

    // True when the arc is so small that the native canvas draws the whole
    // ellipse instead (Tk on Windows draws the arc through GDI, which rounds
    // the arc's end points to whole pixels).
    bool DegeneratesToFullEllipse() const;

    double ArcDistanceTo(Point p) const;
};
