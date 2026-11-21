#include "FontCache.h"

#include <cstdio>

#include "Utf16.h"

FontCache& FontCache::Instance() {
    static FontCache cache;
    return cache;
}

std::string FontCache::KeyOf(const TextStyle& style) const {
    std::string key;
    BuildKey(style, key);
    return key;
}

void FontCache::BuildKey(const TextStyle& style, std::string& out) {
    // The key is built by hand: a string stream would allocate for every
    // text item, and a repaint draws a lot of text.
    out.clear();
    out.append(style.fontFamily);
    char buffer[64];
    int written = std::snprintf(buffer, sizeof(buffer), "@%g%s%s",
                                style.size,
                                style.bold ? "#b" : "",
                                style.italic ? "#i" : "");
    if (written > 0) out.append(buffer, static_cast<std::size_t>(written));
}

Microsoft::WRL::ComPtr<IDWriteTextFormat> FontCache::GetFormat(IDWriteFactory* writeFactory,
                                                                const TextStyle& style) {
    if (!writeFactory) return nullptr;

    // The key buffer is a member: looking a text format up must not
    // allocate, otherwise every text item of every repaint would.
    BuildKey(style, key_);
    auto it = formats_.find(key_);
    if (it != formats_.end()) return it->second;

    const std::wstring wideFamily = Utf8ToWide(style.fontFamily);

    Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
    const HRESULT hr = writeFactory->CreateTextFormat(
        wideFamily.c_str(),
        nullptr,  // default system font collection
        style.bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
        style.italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        static_cast<FLOAT>(style.size),
        L"en-us",
        &format);
    if (FAILED(hr)) return nullptr;

    formats_.emplace(key_, format);
    return format;
}
