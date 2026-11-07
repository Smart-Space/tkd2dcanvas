// BrushCache.h — caches SolidBrush-like style keys. See PenCache for rationale.
#pragma once

#include <string>

#include "tkd2dcanvas/Renderer.h"

class BrushCache {
public:
    static BrushCache& Instance();
    std::string KeyOf(const BrushStyle& style) const;

private:
    BrushCache() = default;
};
