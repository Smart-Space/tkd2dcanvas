#include "tkd2dcanvas/TagsIndex.h"

#include <algorithm>

void TagsIndex::Add(int itemId, const std::string& tag) {
    if (tag.empty()) return;
    if (Contains(itemId, tag)) return;
    byTag_.emplace(tag, itemId);
    byItem_.emplace(itemId, tag);
}

void TagsIndex::Remove(int itemId, const std::string& tag) {
    auto range = byTag_.equal_range(tag);
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second == itemId) {
            byTag_.erase(it);
            break;
        }
    }
    auto itemRange = byItem_.equal_range(itemId);
    for (auto it = itemRange.first; it != itemRange.second; ++it) {
        if (it->second == tag) {
            byItem_.erase(it);
            break;
        }
    }
}

void TagsIndex::RemoveItem(int itemId) {
    auto itemRange = byItem_.equal_range(itemId);
    // Collect tags first; erasing while iterating the equal range is unsafe.
    std::vector<std::string> tags;
    for (auto it = itemRange.first; it != itemRange.second; ++it) tags.push_back(it->second);
    for (const auto& tag : tags) {
        auto range = byTag_.equal_range(tag);
        for (auto it = range.first; it != range.second;) {
            if (it->second == itemId) it = byTag_.erase(it);
            else ++it;
        }
    }
    byItem_.erase(itemRange.first, itemRange.second);
}

void TagsIndex::Clear() {
    byTag_.clear();
    byItem_.clear();
}

std::vector<int> TagsIndex::Find(const std::string& tag) const {
    std::vector<int> ids;
    auto range = byTag_.equal_range(tag);
    for (auto it = range.first; it != range.second; ++it) ids.push_back(it->second);
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
}

std::vector<std::string> TagsIndex::TagsOf(int itemId) const {
    std::vector<std::string> tags;
    auto range = byItem_.equal_range(itemId);
    for (auto it = range.first; it != range.second; ++it) tags.push_back(it->second);
    return tags;
}

bool TagsIndex::Contains(int itemId, const std::string& tag) const {
    auto range = byTag_.equal_range(tag);
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second == itemId) return true;
    }
    return false;
}
