#include "tkd2dcanvas/CanvasItem.h"

#include <algorithm>

#include "tkd2dcanvas/HitTest.h"

bool CanvasItem::HitTest(Point p, double tolerance) const {
    const BBox box = GetBBox();
    return HitTest::PointInRect(p, box.left, box.top, box.right, box.bottom, tolerance);
}

bool CanvasItem::HasTag(const std::string& tag) const {
    return std::find(tags_.begin(), tags_.end(), tag) != tags_.end();
}

void CanvasItem::AddTag(const std::string& tag) {
    if (!HasTag(tag)) tags_.push_back(tag);
}

bool CanvasItem::RemoveTag(const std::string& tag) {
    auto it = std::find(tags_.begin(), tags_.end(), tag);
    if (it == tags_.end()) return false;
    tags_.erase(it);
    return true;
}
