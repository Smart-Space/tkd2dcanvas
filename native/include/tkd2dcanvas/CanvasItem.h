// CanvasItem.h — base class for all canvas items. No external dependencies.
#pragma once

#include <string>
#include <vector>
#include <memory>

#include "tkd2dcanvas/BBox.h"
#include "tkd2dcanvas/Geometry.h"
#include "tkd2dcanvas/Renderer.h"
#include "tkd2dcanvas/ItemStyle.h"

class CanvasItem {
public:
    using Id = int;

    virtual ~CanvasItem() = default;

    CanvasItem(Id id, std::string type) : id_(id), type_(std::move(type)) {}

    Id GetId() const { return id_; }
    // Assigned by CanvasModel when the item is created.
    void SetId(Id id) { id_ = id; }
    const std::string& Type() const { return type_; }

    // --- Geometry ----------------------------------------------------------
    virtual BBox GetBBox() const = 0;
    virtual void Translate(double dx, double dy) = 0;

    // Distance from `p` to the item's outline; 0 when the point lies inside a
    // filled area of the item. The default implementation measures the
    // distance to the bounding box. Items with a real outline (rectangle,
    // oval, line, polygon, arc) refine this.
    virtual double DistanceTo(Point p) const;

    // Classification of the item against the probe rectangle ({x1,y1,x2,y2}
    // with x1 <= x2, y1 <= y2), mirroring Tk's item areaProcs used by
    // `find overlapping` / `find enclosed`:
    //   -1 the item is entirely outside the rectangle,
    //    0 the item overlaps the rectangle,
    //    1 the item is entirely inside the rectangle.
    // The default implementation classifies by bounding box; items with a
    // precise answer (line) refine this.
    virtual int AreaOverlap(const double rect[4]) const;

    // True when `p` is within `tolerance` of the item. The outline width
    // widens the hit area, exactly like Tk (the stroke covers the path plus
    // half its width on each side).
    bool HitTest(Point p, double tolerance) const {
        double distance = DistanceTo(p);
        const double halfWidth = Style().width * 0.5;
        if (halfWidth > 0.0) distance -= halfWidth;
        if (distance < 0.0) distance = 0.0;
        return distance <= tolerance;
    }

    // --- Rendering ---------------------------------------------------------
    virtual void Draw(Renderer& renderer) const = 0;

    // Visual style shared by all items (-fill/-outline/-width/-dash).
    ItemStyle& Style() { return style_; }
    const ItemStyle& Style() const { return style_; }

    // --- Coordinates ---------------------------------------------------------
    // Flat Tk-style coordinate list ("x1 y1 x2 y2 ..."). Items that do not
    // support coordinates (window items, ...) keep the defaults.
    virtual std::vector<double> GetCoordinates() const { return {}; }
    // Sets coordinates from a flat list. False = nothing changed.
    virtual bool SetCoordinates(const std::vector<double>& coords) {
        (void)coords;
        return false;
    }

    // --- Tags --------------------------------------------------------------
    const std::vector<std::string>& Tags() const { return tags_; }
    bool HasTag(const std::string& tag) const;
    void AddTag(const std::string& tag);
    bool RemoveTag(const std::string& tag);
    void ClearTags() { tags_.clear(); }

private:
    Id id_;
    std::string type_;
    std::vector<std::string> tags_;
    ItemStyle style_;
};

using CanvasItemPtr = std::unique_ptr<CanvasItem>;
