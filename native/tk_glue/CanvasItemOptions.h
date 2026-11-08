// CanvasItemOptions.h — parsing and application of canvas *item* options
// (-fill / -outline / -width / -dash / -dashoffset / -tags / ...). Widget-level
// options live in CanvasOptions.*; item options attach to C++ items created
// through ItemFactory. No direct D2D/Win32 dependencies.
#pragma once

#include <tcl.h>
#include <tk.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "tkd2dcanvas/Color.h"
#include "tkd2dcanvas/DashPattern.h"
#include "tkd2dcanvas/Renderer.h"
#include "tkd2dcanvas/ItemStyle.h"
#include "tkd2dcanvas/SmoothCurve.h"
#include "CanvasTkFont.h"

class CanvasItem;  // global namespace (tkd2dcanvas/CanvasItem.h)

namespace tkd2dcanvas {

struct CanvasWidget;

// A fully-validated set of item option changes. Only the members whose
// `hasXxx` flag is set are applied.
struct ItemConfig {
    bool hasFill = false;
    std::optional<Color> fill;        // empty = painting disabled
    bool hasOutline = false;
    std::optional<Color> outline;     // empty = painting disabled
    bool hasWidth = false;
    double width = 1.0;
    bool hasDash = false;
    DashPattern dash;
    bool hasTags = false;
    std::vector<std::string> tags;    // replaces the item's tag set
    // Line/polygon-only option (-joinstyle); Tk defaults it to round.
    bool hasJoinStyle = false;
    JoinStyle joinStyle = JoinStyle::Round;
    // Line-only option (-capstyle); Tk defaults it to butt (Flat).
    bool hasCapStyle = false;
    CapStyle capStyle = CapStyle::Flat;
    // Arc-only options (-start/-extent/-style); other item types reject them.
    bool hasStart = false;
    double start = 0.0;               // degrees
    bool hasExtent = false;
    double extent = 90.0;             // degrees
    bool hasStyle = false;
    std::string style;                // normalized: "pieslice" | "chord" | "arc"
    // Line/polygon-only options (-smooth/-splinesteps).
    bool hasSmooth = false;
    SmoothMode smooth = SmoothMode::None;  // None = straight segments
    bool hasSplineSteps = false;
    int splineSteps = kDefaultSplineSteps;
    bool hasDashOffset = false;
    double dashOffset = 0.0;
    // Image items (-image; check-only validation happens during parsing).
    bool hasImage = false;
    std::string imageName;                          // "" clears the reference
    // Bitmap items (-bitmap; the bits are decoded once during parsing so a
    // bad value aborts the command before anything is applied).
    bool hasBitmap = false;
    std::string bitmapName;
    bool bitmapResolved = false;                    // set when the XBM bits were decoded OK
    int bitmapWidth = 0;
    int bitmapHeight = 0;
    std::vector<std::uint8_t> bitmapBits;
    // Bitmap-only paint options
    bool hasForeground = false;
    std::optional<Color> foreground;
    bool hasBackground = false;
    std::optional<Color> background;
    // -angle (text items): counter-clockwise rotation in degrees about the
    // item's anchor point; normalized to [0, 360) by the item.
    bool hasAngle = false;
    double angle = 0.0;
    bool hasState = false;
    ItemState state = ItemState::Normal;
    bool hasDisabledFill = false;
    std::optional<Color> disabledFill;
    // Text items (-text/-font/-justify/-wrap). -width reuses hasWidth/width
    // and means "wrap length in canvas units" on text items (Tk semantics).
    bool hasText = false;
    std::string text;
    bool hasFont = false;
    std::string fontSpec;                           // raw spec, as given
    bool fontResolved = false;
    TkFontInfo fontInfo;                            // `font actual` result for -font
    // Anchor (text / image / bitmap items); canonical short form.
    bool hasAnchor = false;
    std::string anchor;
    bool hasJustify = false;
    std::string justify;                            // left|center|right
    bool hasWrap = false;
    std::string wrap;                               // word|char|none
};

// Parses "-option value" pairs from objv[firstArg, objc). Returns false with
// the interpreter error set on the first invalid pair (nothing is applied).
bool ParseItemConfig(Tcl_Interp* interp, CanvasWidget& widget,
                     int firstArg, int objc, Tcl_Obj* const objv[],
                     ItemConfig& out);

// Checks that the parsed options make sense for `item` (arc-only options are
// rejected on other item types). Returns true when the config is applicable.
bool CheckItemConfig(Tcl_Interp* interp, const CanvasItem& item,
                     const ItemConfig& config);

// Applies a validated config to one item (including tag replacement).
// Cannot fail; call ParseItemConfig first.
void ApplyItemConfig(CanvasWidget& widget, int itemId,
                     CanvasItem& item, const ItemConfig& config);

// Formats one option's current value as a string (e.g. "#ff0000", "3",
// "8 4", "a b"). Used by itemcget and the itemconfigure report.
std::string ItemOptionValueString(const CanvasItem& item, const char* option);

// Builds the itemconfigure no-argument report: a dict of
// "-fill"/"-outline"/"-width"/"-dash"/"-tags" -> current value.
Tcl_Obj* ItemConfigReport(Tcl_Interp* interp, CanvasItem& item);

}  // namespace tkd2dcanvas
