// FontCache.h — caches IDWriteTextFormat objects (family/size/weight/style).
// Underline/strikethrough are applied per text layout, not on the format.
#pragma once

#include <string>
#include <unordered_map>
#include <wrl/client.h>

#include <dwrite.h>

#include "tkd2dcanvas/Renderer.h"

class FontCache {
public:
    static FontCache& Instance();

    Microsoft::WRL::ComPtr<IDWriteTextFormat> GetFormat(IDWriteFactory* writeFactory,
                                                        const TextStyle& style);

    std::string KeyOf(const TextStyle& style) const;

    void Clear() { formats_.clear(); }

private:
    FontCache() = default;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<IDWriteTextFormat>> formats_;
};
