#include "tkd2dcanvas/Color.h"

#include <cctype>
#include <cstdio>
#include <unordered_map>

namespace {

bool HexDigit(char c, int& value) {
    if (c >= '0' && c <= '9') { value = c - '0'; return true; }
    if (c >= 'a' && c <= 'f') { value = c - 'a' + 10; return true; }
    if (c >= 'A' && c <= 'F') { value = c - 'A' + 10; return true; }
    return false;
}

bool ParseHexChannel(std::string_view s, std::size_t i, std::uint8_t& out) {
    int hi = 0, lo = 0;
    if (i + 1 >= s.size()) return false;
    if (!HexDigit(s[i], hi) || !HexDigit(s[i + 1], lo)) return false;
    out = static_cast<std::uint8_t>((hi << 4) | lo);
    return true;
}

// Small built-in set (Tk supports the full X11 rgb.txt; the rest is resolved
// by the platform layer / CanvasTkColor).
const std::unordered_map<std::string, Color>& NamedColors() {
    static const std::unordered_map<std::string, Color> kColors = {
        {"black", {0, 0, 0}},      {"white", {255, 255, 255}}, {"red", {255, 0, 0}},
        {"green", {0, 128, 0}},    {"lime", {0, 255, 0}},      {"blue", {0, 0, 255}},
        {"yellow", {255, 255, 0}}, {"cyan", {0, 255, 255}},    {"magenta", {255, 0, 255}},
        {"gray", {128, 128, 128}}, {"grey", {128, 128, 128}},  {"orange", {255, 165, 0}},
        {"purple", {128, 0, 128}}, {"brown", {165, 42, 42}},   {"pink", {255, 192, 203}},
        {"navy", {0, 0, 128}},     {"teal", {0, 128, 128}},    {"olive", {128, 128, 0}},
        {"silver", {192, 192, 192}}, {"maroon", {128, 0, 0}},
        {"tomato", {255, 99, 71}}, {"lightblue", {173, 216, 230}}, {"lightgray", {211, 211, 211}},
        {"lightgrey", {211, 211, 211}},
    };
    return kColors;
}

}  // namespace

bool Color::TryParseHex(std::string_view text, Color& out) {
    if (text.size() >= 1 && text[0] == '#') text.remove_prefix(1);
    else return false;

    auto expand = [](char c) { return static_cast<std::uint8_t>(((c - '0') * 17)); };

    if (text.size() == 3 || text.size() == 4) {
        int v[4] = {0, 0, 0, 15};
        for (std::size_t i = 0; i < text.size(); ++i) {
            if (!HexDigit(text[i], v[i])) return false;
        }
        out = Color{static_cast<std::uint8_t>(v[0] * 17), static_cast<std::uint8_t>(v[1] * 17),
                    static_cast<std::uint8_t>(v[2] * 17), static_cast<std::uint8_t>(v[3] * 17)};
        return true;
    }
    if (text.size() == 6 || text.size() == 8) {
        std::uint8_t ch[4] = {0, 0, 0, 255};
        for (std::size_t i = 0; i < text.size() / 2; ++i) {
            if (!ParseHexChannel(text, i * 2, ch[i])) return false;
        }
        out = Color{ch[0], ch[1], ch[2], ch[3]};
        return true;
    }
    return false;
}

bool Color::TryParse(std::string_view text, Color& out) {
    if (TryParseHex(text, out)) return true;

    // Case-insensitive name lookup.
    std::string name;
    name.reserve(text.size());
    for (char c : text) name += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    auto it = NamedColors().find(name);
    if (it == NamedColors().end()) return false;
    out = it->second;
    return true;
}

std::string Color::ToHex() const {
    char buf[16];
    if (a == 255) {
        std::snprintf(buf, sizeof buf, "#%02x%02x%02x", r, g, b);
    } else {
        std::snprintf(buf, sizeof buf, "#%02x%02x%02x%02x", r, g, b, a);
    }
    return std::string(buf);
}
