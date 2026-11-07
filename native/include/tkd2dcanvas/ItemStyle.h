// ItemStyle.h — visual style shared by canvas items (-fill/-outline/-width/-dash).
// Core layer: no dependencies beyond Color/DashPattern/Renderer.
#pragma once

#include <optional>

#include "tkd2dcanvas/Color.h"
#include "tkd2dcanvas/DashPattern.h"
#include "tkd2dcanvas/Renderer.h"

struct ItemStyle {
    // Empty = not filled / no outline (Tk spells this -fill "" / -outline "").
    std::optional<Color> fill;
    std::optional<Color> outline;
    double width = 1.0;          // pen width in canvas units (Tk -width)
    DashPattern dash;            // solid by default (Tk -dash)
};

// Builds a PenStyle from the style's outline-ish paint channel.
// Callers decide which color channel (fill for lines, outline for shapes).
inline PenStyle ItemPenStyle(const Color& color, const ItemStyle& style,
                             CapStyle cap = CapStyle::Flat,
                             JoinStyle join = JoinStyle::Miter) {
    PenStyle pen;
    pen.color = color;
    pen.width = style.width;
    pen.dash = style.dash;
    pen.cap = cap;
    pen.join = join;
    pen.antialias = true;
    return pen;
}
