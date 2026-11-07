#include "tkd2dcanvas/ItemFactory.h"

#include <algorithm>
#include <stdexcept>

ItemFactory& ItemFactory::Instance() {
    static ItemFactory factory;
    return factory;
}

void ItemFactory::Register(std::string type, Creator creator) {
    creators_[std::move(type)] = std::move(creator);
}

bool ItemFactory::Unregister(std::string_view type) {
    return creators_.erase(std::string(type)) > 0;
}

CanvasItemPtr ItemFactory::Create(std::string_view type, CanvasItem::Id id) const {
    auto it = creators_.find(std::string(type));
    if (it == creators_.end()) {
        throw std::invalid_argument("unknown canvas item type: " + std::string(type));
    }
    return it->second(id);
}

bool ItemFactory::IsRegistered(std::string_view type) const {
    return creators_.find(std::string(type)) != creators_.end();
}

std::vector<std::string> ItemFactory::RegisteredTypes() const {
    std::vector<std::string> types;
    types.reserve(creators_.size());
    for (const auto& [type, _] : creators_) types.push_back(type);
    std::sort(types.begin(), types.end());
    return types;
}
