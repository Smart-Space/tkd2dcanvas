#include "FontCache.h"

#include <sstream>

#include "Utf16.h"

FontCache& FontCache::Instance() {
    static FontCache cache;
    return cache;
}

std::string FontCache::KeyOf(const TextStyle& style) const {
    std::ostringstream oss;
    oss << style.fontFamily << '@' << style.size
        << (style.bold ? "#b" : "") << (style.italic ? "#i" : "");
    return oss.str();
}

Microsoft::WRL::ComPtr<IDWriteTextFormat> FontCache::GetFormat(IDWriteFactory* writeFactory,
                                                                const TextStyle& style) {
    if (!writeFactory) return nullptr;

    const std::string key = KeyOf(style);
    auto it = formats_.find(key);
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

    format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    formats_.emplace(key, format);
    return format;
}
