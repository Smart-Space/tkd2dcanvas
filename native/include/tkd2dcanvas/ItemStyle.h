// ItemStyle.h — visual style shared by canvas items (-fill/-outline/-width/-dash -state/-disabledfill).
// Core layer: no direct Tk/D2D dependencies.
#pragma once

#include <optional>
#include <string>
#include <cstring>

#include "tkd2dcanvas/Color.h"
#include "tkd2dcanvas/DashPattern.h"
#include "tkd2dcanvas/Renderer.h"

enum class ItemState : int { Normal = 0, Disabled = 1, Hidden = 2 };

struct ItemStyle {
    // Empty = not filled / no outline (Tk spells this -fill "" / -outline "").
    std::optional<Color> fill;
    std::optional<Color> outline;
    std::optional<Color> disabledFill;
    double width = 1.0;          // pen width in canvas units (Tk -width)
    DashPattern dash;            // solid by default (Tk -dash)
    double dashOffset = 0.0;     // canvas units into the -dash pattern (Tk -dashoffset)
    JoinStyle join = JoinStyle::Round;
    CapStyle cap = CapStyle::Flat;
    ItemState state = ItemState::Normal;
};

inline bool ItemStateFromName(const std::string& text, ItemState& out) {
    if (text.empty()) {
        out = ItemState::Normal;
        return true;
    }
    auto match = [&](const char* word) {
        return text.size() <= std::strlen(word) &&
               std::strncmp(word, text.c_str(), text.size()) == 0;
    };
    const bool normal = match("normal");
    const bool disabled = match("disabled");
    const bool hidden = match("hidden");
    if (static_cast<int>(normal) + static_cast<int>(disabled) + static_cast<int>(hidden) != 1) {
        return false;
    }
    out = normal ? ItemState::Normal : disabled ? ItemState::Disabled : ItemState::Hidden;
    return true;
}

inline const char* ItemStateName(ItemState state) {
    switch (state) {
        case ItemState::Disabled: return "disabled";
        case ItemState::Hidden: return "hidden";
        default: return "normal";
    }
}

inline std::optional<Color> ItemStateFill(const ItemStyle& style) {
    if (style.state == ItemState::Disabled && style.disabledFill.has_value()) {
        return style.disabledFill;
    }
    return style.fill;
}

// Builds a PenStyle from the style's outline-ish paint channel.
// Callers decide which color channel (fill for lines, outline for shapes).
inline PenStyle ItemPenStyle(const Color& color, const ItemStyle& style,
                             CapStyle cap = CapStyle::Flat,
                             JoinStyle join = JoinStyle::Miter) {
    PenStyle pen;
    pen.color = color;
    pen.width = style.width;
    pen.dash = style.dash;
    pen.dash.offset = static_cast<float>(style.dashOffset);
    pen.cap = cap;
    pen.join = join;
    pen.antialias = true;
    return pen;
}
