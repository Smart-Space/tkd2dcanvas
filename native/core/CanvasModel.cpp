#include "tkd2dcanvas/CanvasModel.h"

#include <algorithm>
#include <numeric>

CanvasModel::CanvasModel() {
    zOrder_.reserve(256);
}

int CanvasModel::CreateItem(CanvasItemPtr item) {
    if (!item) return kInvalidId;

    const int id = nextId_++;
    item->SetId(id);  // caller may pass a provisional id; the model assigns the real one
    items_.emplace(id, std::move(item));
    zOrder_.push_back(id);
    return id;
}

bool CanvasModel::DeleteItem(int id) {
    auto it = items_.find(id);
    if (it == items_.end()) return false;

    for (const auto& tag : it->second->Tags()) tags_.Remove(id, tag);
    // Also drop any stale index entries.
    tags_.RemoveItem(id);

    DetachFromZOrder(id);
    items_.erase(it);
    return true;
}

void CanvasModel::DeleteAll() {
    tags_.Clear();
    items_.clear();
    zOrder_.clear();
}

bool CanvasModel::Exists(int id) const {
    return items_.find(id) != items_.end();
}

CanvasItem* CanvasModel::FindItem(int id) {
    auto it = items_.find(id);
    return it == items_.end() ? nullptr : it->second.get();
}

const CanvasItem* CanvasModel::FindItem(int id) const {
    auto it = items_.find(id);
    return it == items_.end() ? nullptr : it->second.get();
}

bool CanvasModel::RaiseToTop(int id) {
    if (!Exists(id)) return false;
    DetachFromZOrder(id);
    zOrder_.push_back(id);
    return true;
}

bool CanvasModel::LowerToBottom(int id) {
    if (!Exists(id)) return false;
    DetachFromZOrder(id);
    zOrder_.insert(zOrder_.begin(), id);
    return true;
}

bool CanvasModel::RaiseBy(int id, int delta) {
    const auto it = std::find(zOrder_.begin(), zOrder_.end(), id);
    if (it == zOrder_.end()) return false;

    const int from = static_cast<int>(std::distance(zOrder_.begin(), it));
    int to = from + delta;
    to = std::clamp(to, 0, static_cast<int>(zOrder_.size()) - 1);
    if (to == from) return true;

    zOrder_.erase(it);
    zOrder_.insert(zOrder_.begin() + to, id);
    return true;
}

bool CanvasModel::MoveItemsAbove(const std::vector<int>& ids, int anchorId) {
    // Collect the items in display-list order; the relative order of the moved
    // items is preserved (Tk semantics).
    std::vector<int> moved;
    for (int id : zOrder_) {
        if (std::find(ids.begin(), ids.end(), id) != ids.end()) moved.push_back(id);
    }
    if (moved.empty()) return false;

    for (int id : moved) DetachFromZOrder(id);

    // Without an anchor the items go to the top of the display list.
    std::vector<int>::iterator anchor = zOrder_.end();
    if (anchorId != kInvalidId) {
        const auto it = std::find(zOrder_.begin(), zOrder_.end(), anchorId);
        if (it != zOrder_.end()) anchor = it;
    }
    zOrder_.insert(anchor, moved.begin(), moved.end());
    return true;
}

bool CanvasModel::MoveItemsBelow(const std::vector<int>& ids, int anchorId) {
    std::vector<int> moved;
    for (int id : zOrder_) {
        if (std::find(ids.begin(), ids.end(), id) != ids.end()) moved.push_back(id);
    }
    if (moved.empty()) return false;

    for (int id : moved) DetachFromZOrder(id);

    // Without an anchor the items go to the bottom of the display list.
    std::vector<int>::iterator anchor = zOrder_.begin();
    if (anchorId != kInvalidId) {
        const auto it = std::find(zOrder_.begin(), zOrder_.end(), anchorId);
        if (it != zOrder_.end()) anchor = it;
    }
    zOrder_.insert(anchor, moved.begin(), moved.end());
    return true;
}

std::vector<CanvasItem*> CanvasModel::ItemsInZOrder() const {
    std::vector<CanvasItem*> result;
    result.reserve(zOrder_.size());
    for (int id : zOrder_) {
        auto it = items_.find(id);
        if (it != items_.end()) result.push_back(it->second.get());
    }
    return result;
}

bool CanvasModel::Translate(const std::vector<int>& ids, double dx, double dy) {
    bool any = false;
    for (int id : ids) {
        if (auto* item = FindItem(id)) {
            item->Translate(dx, dy);
            any = true;
        }
    }
    return any;
}

BBox CanvasModel::ItemBBox(int id) const {
    const auto* item = FindItem(id);
    return item ? item->GetBBox() : BBox::Empty();
}

BBox CanvasModel::AllBBox() const {
    BBox result = BBox::Empty();
    for (const auto& [id, item] : items_) result = result.Union(item->GetBBox());
    return result;
}

int CanvasModel::FindItemAt(double x, double y, double tolerance, bool skipDisabled) const {
    const Point p{x, y};
    // Topmost first: the last item in the display list wins.
    for (auto it = zOrder_.rbegin(); it != zOrder_.rend(); ++it) {
        const auto* item = FindItem(*it);
        if (!item) continue;
        if (item->IsHidden()) continue;
        if (skipDisabled && item->IsDisabled()) continue;
        if (item->HitTest(p, tolerance)) return *it;
    }
    return kInvalidId;
}

int CanvasModel::FindClosest(double x, double y, double halo) const {
    return FindClosestFrom(x, y, halo, kInvalidId);
}

int CanvasModel::FindClosestFrom(double x, double y, double halo, int startId) const {
    if (zOrder_.empty()) return kInvalidId;

    // The search starts at `startId` and wraps around the display list.
    std::size_t start = 0;
    if (startId != kInvalidId) {
        const auto it = std::find(zOrder_.begin(), zOrder_.end(), startId);
        if (it != zOrder_.end()) {
            start = static_cast<std::size_t>(std::distance(zOrder_.begin(), it));
        }
    }

    const Point p{x, y};
    int best = kInvalidId;
    double bestDist = 0.0;
    for (std::size_t i = 0; i < zOrder_.size(); ++i) {
        const int id = zOrder_[(start + i) % zOrder_.size()];
        const auto* item = FindItem(id);
        if (!item) continue;
        if (item->IsHidden()) continue;
        double dist = item->DistanceTo(p) - halo;
        if (dist < 0.0) dist = 0.0;
        // "<=" makes the item found last win a tie (Tk behaves the same way).
        if (best == kInvalidId || dist <= bestDist) {
            best = id;
            bestDist = dist;
        }
    }
    return best;
}

bool CanvasModel::AddTag(int id, const std::string& tag) {
    auto* item = FindItem(id);
    if (!item) return false;
    item->AddTag(tag);
    tags_.Add(id, tag);
    return true;
}

bool CanvasModel::AddTagToItems(const std::vector<int>& ids, const std::string& tag) {
    bool any = false;
    for (int id : ids) any = AddTag(id, tag) || any;
    return any;
}

bool CanvasModel::RemoveTag(int id, const std::string& tag) {
    auto* item = FindItem(id);
    if (!item) return false;
    item->RemoveTag(tag);
    tags_.Remove(id, tag);
    return true;
}

bool CanvasModel::RemoveTagFromItems(const std::vector<int>& ids, const std::string& tag) {
    bool any = false;
    for (int id : ids) any = RemoveTag(id, tag) || any;
    return any;
}

std::vector<int> CanvasModel::FindWithTag(const std::string& tag) const {
    // Tag "all" is a built-in Tk-style alias.
    if (tag == "all") return zOrder_;
    return tags_.Find(tag);
}

std::vector<std::string> CanvasModel::TagsOf(int id) const {
    // The item keeps its tags in insertion order (like Tk); the index is only
    // used for lookups.
    const auto* item = FindItem(id);
    if (!item) return {};
    return item->Tags();
}

void CanvasModel::DetachFromZOrder(int id) {
    auto it = std::find(zOrder_.begin(), zOrder_.end(), id);
    if (it != zOrder_.end()) zOrder_.erase(it);
}
