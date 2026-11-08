// Anchor.h — Tk anchor parsing and anchor-based block offsets.
// Core layer: no external dependencies. Tk anchors:
//   n, ne, e, se, s, sw, w, nw, c (alias of center).
// Matching is prefix-based and case-insensitive like Tk's Tk_GetAnchor.
#pragma once

#include <cmath>
#include <string>
#include <string_view>

#include "tkd2dcanvas/Geometry.h"

// Normalizes an anchor string to its canonical short form
// ("n", "ne", "e", "se", "s", "sw", "w", "nw", "center").
// Returns false when the string does not match any anchor (even as a
// prefix). "" is not a valid anchor here; Tk treats an empty anchor as an
// error for -anchor options.
inline bool NormalizeAnchor(const char* raw, std::string& out) {
    if (!raw) return false;
    std::string text = raw;
    for (auto& c : text) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    const std::size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return false;
    const std::size_t end = text.find_last_not_of(" \t\r\n");
    text = text.substr(begin, end - begin + 1);

    // "c" and "center" map to Tk's compound "center" anchor; everything else
    // prefix-matches the compass points like Tk.
    const std::pair<const char*, const char*> table[] = {
        {"center", "center"},
        {"n", "n"}, {"ne", "ne"}, {"e", "e"}, {"se", "se"}, {"s", "s"},
        {"sw", "sw"}, {"w", "w"}, {"nw", "nw"},
    };
    for (const auto& entry : table) {
        const std::string keyword = entry.first;
        if (text == keyword) {
            out = entry.second;
            return true;
        }
    }
    int matches = 0;
    const char* chosen = nullptr;
    for (const auto& entry : table) {
        const std::string keyword = entry.first;
        if (text.size() < keyword.size() &&
            keyword.compare(0, text.size(), text) == 0) {
            ++matches;
            chosen = entry.second;
        }
    }
    if (matches == 1 && chosen) {
        out = chosen;
        return true;
    }
    return false;
}

// Tk's exact int conversions to mirror its bbox output.
// (int)(d + 0.5) for d >= 0, otherwise (int)ceil(d - 0.5): round half away
// from zero — used by Tk's image/bitmap bbox code.
inline double TkAnchorRound(double v) {
    return v >= 0 ? std::floor(v + 0.5) : std::ceil(v - 0.5);
}

// Tk's ROUND() macro in tkCanvText.c: floor(d + 0.5) for everything.
inline double TkRoundHalf(double v) {
    return std::floor(v + 0.5);
}

// Tk does the anchor-offset arithmetic in ints (Tk image/bitmap code uses
// `width / 2` with truncation toward zero), and text items pass int
// width/height down to the layout; match that exactly, even though our
// stored geometry is double.
inline int AnchorIntTrunc(double v) {
    return v >= 0 ? static_cast<int>(v) : -static_cast<int>(-v);
}

// Horizontal offset applied to a block of width `width` anchored at (x, y):
// negative or zero. "w"/"nw"/"sw" -> 0, "e"/"ne"/"se" -> -width, else centered.
// Anchors are the canonical forms from NormalizeAnchor ("c" is never stored).
inline double AnchorOffsetX(const std::string& anchor, double width) {
    const int w = AnchorIntTrunc(width);
    if (anchor == "w" || anchor == "nw" || anchor == "sw") return 0.0;
    if (anchor == "e" || anchor == "ne" || anchor == "se") return -static_cast<double>(w);
    return -static_cast<double>(w / 2);
}

// Vertical offset applied to a block of height `height` anchored at (x, y):
// "n"/"ne"/"nw" -> 0, "s"/"se"/"sw" -> -height, else (middle) centered.
inline double AnchorOffsetY(const std::string& anchor, double height) {
    const int h = AnchorIntTrunc(height);
    if (anchor == "n" || anchor == "ne" || anchor == "nw") return 0.0;
    if (anchor == "s" || anchor == "se" || anchor == "sw") return -static_cast<double>(h);
    return -static_cast<double>(h / 2);
}

inline Point AnchorTopLeft(const std::string& anchor, double x, double y, double width, double height) {
    return Point{x + AnchorOffsetX(anchor, width), y + AnchorOffsetY(anchor, height)};
}

// Horizontal offset applied to a single line of width `lineWidth` inside a
// block of width `blockWidth`, for Tk -justify semantics:
//   left:   lines start at the block's left edge,
//   center: each line is centered within the block,
//   right:  each line is flush with the block's right edge.
inline double JustifyOffset(const std::string& justify, double blockWidth, double lineWidth) {
    if (justify == "center") return (blockWidth - lineWidth) * 0.5;
    if (justify == "right") return blockWidth - lineWidth;
    return 0.0;
}

// Normalizes a -justify value (prefix-matched, case-insensitive) to
// "left"/"center"/"right". Returns false for anything else.
inline bool NormalizeJustify(const char* raw, std::string& out) {
    if (!raw) return false;
    std::string text = raw;
    for (auto& c : text) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    const std::size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return false;
    const std::size_t end = text.find_last_not_of(" \t\r\n");
    text = text.substr(begin, end - begin + 1);
    const char* keywords[] = {"left", "center", "right"};
    for (const char* keyword : keywords) {
        const std::size_t len = std::string_view(keyword).size();
        if (text.size() <= len && std::string_view(keyword).compare(0, text.size(), text) == 0) {
            out = keyword;
            return true;
        }
    }
    return false;
}

// Normalizes a -wrap value to "word"/"char"/"none". Returns false for
// anything else. (A canvas-text extension: Tk 8.6 canvas text has no -wrap
// option; the wrapping granularity follows Tk's text widget instead.)
inline bool NormalizeWrap(const char* raw, std::string& out) {
    if (!raw) return false;
    std::string text = raw;
    for (auto& c : text) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    const std::size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return false;
    const std::size_t end = text.find_last_not_of(" \t\r\n");
    text = text.substr(begin, end - begin + 1);
    const char* keywords[] = {"word", "char", "none"};
    for (const char* keyword : keywords) {
        const std::size_t len = std::string_view(keyword).size();
        if (text.size() <= len && std::string_view(keyword).compare(0, text.size(), text) == 0) {
            out = keyword;
            return true;
        }
    }
    return false;
}
