#include "BrushCache.h"

#include <sstream>

BrushCache& BrushCache::Instance() {
    static BrushCache cache;
    return cache;
}

std::string BrushCache::KeyOf(const BrushStyle& style) const {
    return style.color.ToHex();
}
