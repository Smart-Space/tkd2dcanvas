// TagsIndex.h — tag -> item / item -> tag index maintained by CanvasModel.
#pragma once

#include <string>
#include <vector>
#include <unordered_map>

class TagsIndex {
public:
    void Add(int itemId, const std::string& tag);
    void Remove(int itemId, const std::string& tag);
    void RemoveItem(int itemId);          // drops every tag entry for the item
    void Clear();

    std::vector<int> Find(const std::string& tag) const;
    std::vector<std::string> TagsOf(int itemId) const;
    bool Contains(int itemId, const std::string& tag) const;

    std::size_t Size() const { return byTag_.size(); }

private:
    std::unordered_multimap<std::string, int> byTag_;
    std::unordered_multimap<int, std::string> byItem_;
};
