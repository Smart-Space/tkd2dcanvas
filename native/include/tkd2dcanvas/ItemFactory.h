// ItemFactory.h — creates CanvasItem instances from Tk item type names.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "tkd2dcanvas/CanvasItem.h"

class ItemFactory {
public:
    using Creator = std::function<CanvasItemPtr(CanvasItem::Id)>;

    static ItemFactory& Instance();

    void Register(std::string type, Creator creator);
    bool Unregister(std::string_view type);

    // Throws std::invalid_argument on unknown type.
    CanvasItemPtr Create(std::string_view type, CanvasItem::Id id) const;

    bool IsRegistered(std::string_view type) const;
    std::vector<std::string> RegisteredTypes() const;

private:
    ItemFactory() = default;
    std::unordered_map<std::string, Creator> creators_;
};
