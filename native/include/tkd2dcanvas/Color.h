// Color.h — simple RGBA color value. No external dependencies.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

struct Color {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;

    constexpr Color() = default;
    constexpr Color(std::uint8_t r_, std::uint8_t g_, std::uint8_t b_, std::uint8_t a_ = 255)
        : r(r_), g(g_), b(b_), a(a_) {}

    static Color FromRgba(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255) {
        return Color{r, g, b, a};
    }

    // Parse "#rgb", "#rgba", "#rrggbb", "#rrggbbaa". Returns false on malformed input.
    static bool TryParseHex(std::string_view text, Color& out);

    // Parse a Tk-style color string that may be a system color name.
    // Names are resolved by the platform layer (e.g. CanvasTkColor); this only
    // handles hex forms and the small built-in name set in Color.cpp.
    static bool TryParse(std::string_view text, Color& out);

    bool operator==(const Color& o) const { return r == o.r && g == o.g && b == o.b && a == o.a; }
    bool operator!=(const Color& o) const { return !(*this == o); }

    std::string ToHex() const;  // "#rrggbb" (alpha appended as aa when not 255)
};
