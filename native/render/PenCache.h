// PenCache.h — caches ID2D1StrokeStyle objects keyed by
// color/width/dash/cap/join. Colors are NOT baked into stroke styles (the
// brush carries the color), so the cache key excludes color.
#pragma once

#include <string>
#include <unordered_map>
#include <wrl/client.h>

#include <d2d1.h>

#include "tkd2dcanvas/Renderer.h"

class PenCache {
public:
    static PenCache& Instance();

    // Returns a cached stroke style for the given pen style (created on first
    // use). Cannot fail on normal inputs; returns nullptr only when D2D
    // refuses to create the style.
    Microsoft::WRL::ComPtr<ID2D1StrokeStyle> GetStrokeStyle(ID2D1Factory* factory,
                                                            const PenStyle& style);

    // Legacy key string (kept for diagnostics / tests).
    std::string KeyOf(const PenStyle& style) const;

    void Clear() { styles_.clear(); }

private:
    PenCache() = default;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID2D1StrokeStyle>> styles_;
    // The key of the last lookup.  It is a member so that looking a stroke
    // style up does not allocate (a repaint draws many strokes).
    std::string key_;

    static void BuildKey(const PenStyle& style, std::string& out);
};
