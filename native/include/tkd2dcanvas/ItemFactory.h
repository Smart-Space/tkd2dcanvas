// ItemFactory.h — creates CanvasItem instances from Tk item type names.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "tkd2dcanvas/CanvasItem.h"

class ItemFactory {
public:
    // Creators receive the provisional item id and the flat coordinate list
    // parsed by the caller (`.c create rectangle x1 y1 x2 y2 ...`). They are
    // expected to throw std::invalid_argument when the coordinate count does
    // not match the item type.
    using Creator = std::function<CanvasItemPtr(CanvasItem::Id, const std::vector<double>&)>;

    static ItemFactory& Instance();

    void Register(std::string type, Creator creator);
    bool Unregister(std::string_view type);

    // Throws std::invalid_argument on unknown type or malformed coordinates.
    CanvasItemPtr Create(std::string_view type, CanvasItem::Id id, const std::vector<double>& coords) const;

    bool IsRegistered(std::string_view type) const;
    std::vector<std::string> RegisteredTypes() const;

private:
    ItemFactory() = default;
    std::unordered_map<std::string, Creator> creators_;
};
