#include "tkd2dcanvas/CanvasItem.h"

#include <algorithm>
#include <cmath>

double CanvasItem::DistanceTo(Point p) const {
    const BBox box = GetBBox();
    if (!box.IsValid()) return 0.0;

    const double dx = (p.x < box.left) ? box.left - p.x
                     : (p.x > box.right) ? p.x - box.right
                                         : 0.0;
    const double dy = (p.y < box.top) ? box.top - p.y
                     : (p.y > box.bottom) ? p.y - box.bottom
                                          : 0.0;
    return std::sqrt(dx * dx + dy * dy);
}

int CanvasItem::AreaOverlap(const double rect[4]) const {
    const BBox box = GetBBox();
    if (!box.IsValid()) return -1;
    const bool inside = box.left >= rect[0] && box.top >= rect[1]
        && box.right <= rect[2] && box.bottom <= rect[3];
    if (inside) return 1;
    const bool disjoint = box.left > rect[2] || box.right < rect[0]
        || box.top > rect[3] || box.bottom < rect[1];
    return disjoint ? -1 : 0;
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
