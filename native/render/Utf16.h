// Utf16.h — minimal UTF-8 -> UTF-16 conversion (render layer helpers).
#pragma once

#include <windows.h>

#include <string>

inline std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) return {};

    const int needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                           text.data(), static_cast<int>(text.size()),
                                           nullptr, 0);
    if (needed <= 0) {
        // Lossy fallback: map bytes 1:1 (valid for pure ASCII / latin-1-ish input).
        return std::wstring(text.begin(), text.end());
    }

    std::wstring wide(static_cast<std::size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                        text.data(), static_cast<int>(text.size()),
                        wide.data(), needed);
    return wide;
}
