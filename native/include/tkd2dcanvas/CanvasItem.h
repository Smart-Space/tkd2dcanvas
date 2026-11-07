// CanvasItem.h — base class for all canvas items. No external dependencies.
#pragma once

#include <string>
#include <vector>
#include <memory>

#include "tkd2dcanvas/BBox.h"
#include "tkd2dcanvas/Geometry.h"
#include "tkd2dcanvas/Renderer.h"

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
    virtual bool HitTest(Point p, double tolerance) const;

    // --- Rendering ---------------------------------------------------------
    virtual void Draw(Renderer& renderer) const = 0;

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
};

using CanvasItemPtr = std::unique_ptr<CanvasItem>;
