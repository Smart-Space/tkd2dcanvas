#include "CanvasItemOptions.h"

#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "tkd2dcanvas/CanvasItem.h"
#include "tkd2dcanvas/Anchor.h"

#include "ArcItem.h"
#include "LineItem.h"
#include "PolygonItem.h"
#include "TextItem.h"
#include "ImageItem.h"
#include "BitmapItem.h"
#include "WindowItem.h"
#include "CanvasTkColor.h"
#include "CanvasTkFont.h"
#include "CanvasTkImage.h"
#include "CanvasTextMetrics.h"
#include "CanvasWidget.h"
#include "CanvasTkWindowItem.h"

namespace tkd2dcanvas {

namespace {

// Parses a Tk -dash value. Lenient like the rest of Tk canvas: "", "solid"
// and "none" select a solid line; "."/"dot" and "-"/"dash" select the named
// patterns; anything else is expected to be a Tcl list of dash lengths.
// Malformed input falls back to solid instead of aborting the command.
DashPattern ParseDash(Tcl_Interp* interp, Tcl_Obj* valueObj) {
    const char* raw = Tcl_GetString(valueObj);
    std::string text = raw ? raw : "";
    // Trim surrounding whitespace.
    const std::size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return DashPattern::Solid();
    const std::size_t end = text.find_last_not_of(" \t\r\n");
    text = text.substr(begin, end - begin + 1);

    if (text.empty() || text == "solid" || text == "none") return DashPattern::Solid();
    if (text == "." || text == "dot") return DashPattern::FromList({2.0f, 2.0f});
    if (text == "-" || text == "dash") return DashPattern::FromList({6.0f, 4.0f});

    int length = 0;
    if (Tcl_ListObjLength(interp, valueObj, &length) == TCL_OK && length > 0) {
        std::vector<float> dashes;
        dashes.reserve(static_cast<std::size_t>(length));
        bool ok = true;
        for (int i = 0; i < length; ++i) {
            Tcl_Obj* element = nullptr;
            if (Tcl_ListObjIndex(interp, valueObj, i, &element) != TCL_OK || !element) {
                ok = false;
                break;
            }
            double value = 0.0;
            if (Tcl_GetDoubleFromObj(interp, element, &value) != TCL_OK ||
                !std::isfinite(value) || value <= 0.0) {
                ok = false;
                break;
            }
            dashes.push_back(static_cast<float>(value));
        }
        if (ok) return DashPattern::FromList(dashes);
        Tcl_ResetResult(interp);  // suppress the partial error message
    } else {
        Tcl_ResetResult(interp);
    }
    return DashPattern::Solid();
}

bool ParseColorOption(Tcl_Interp* interp, CanvasWidget& widget,
                      Tcl_Obj* valueObj, std::optional<Color>& out) {
    const char* spec = Tcl_GetString(valueObj);
    if (!spec || *spec == '\0') {
        out.reset();  // -fill "" / -outline "" disables painting
        return true;
    }
    Color color;
    if (!ResolveTkColor(interp, widget.tkwin, spec, color)) return false;
    out = color;
    return true;
}

bool ParseJoinStyle(Tcl_Interp* interp, Tcl_Obj* valueObj, JoinStyle& out) {
    const char* raw = Tcl_GetString(valueObj);
    std::string text = raw ? raw : "";
    const std::size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin != std::string::npos) {
        const std::size_t end = text.find_last_not_of(" \t\r\n");
        text = text.substr(begin, end - begin + 1);
    } else {
        text.clear();
    }
    for (auto& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (text.empty()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("bad join style \"\": must be bevel, miter, or round", -1));
        return false;
    }

    const auto matches = [&](const char* keyword) {
        return text.size() <= std::strlen(keyword) &&
               std::strncmp(keyword, text.c_str(), text.size()) == 0;
    };
    if (matches("bevel")) { out = JoinStyle::Bevel; return true; }
    if (matches("miter")) { out = JoinStyle::Miter; return true; }
    if (matches("round")) { out = JoinStyle::Round; return true; }

    Tcl_SetObjResult(interp, Tcl_ObjPrintf("bad join style \"%s\": must be bevel, miter, or round", text.c_str()));
    return false;
}

bool ParseCapStyle(Tcl_Interp* interp, Tcl_Obj* valueObj, CapStyle& out) {
    const char* raw = Tcl_GetString(valueObj);
    std::string text = raw ? raw : "";
    const std::size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin != std::string::npos) {
        const std::size_t end = text.find_last_not_of(" \t\r\n");
        text = text.substr(begin, end - begin + 1);
    } else {
        text.clear();
    }
    for (auto& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (text.empty()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("bad cap style \"\": must be butt, projecting, or round", -1));
        return false;
    }

    const auto matches = [&](const char* keyword) {
        return text.size() <= std::strlen(keyword) &&
               std::strncmp(keyword, text.c_str(), text.size()) == 0;
    };
    if (matches("butt")) { out = CapStyle::Flat; return true; }
    if (matches("projecting")) { out = CapStyle::Square; return true; }
    if (matches("round")) { out = CapStyle::Round; return true; }

    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
        "bad cap style \"%s\": must be butt, projecting, or round", text.c_str()));
    return false;
}

std::string TrimLower(const std::string& raw) {
    const std::size_t begin = raw.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return {};
    const std::size_t end = raw.find_last_not_of(" \t\r\n");
    std::string out = raw.substr(begin, end - begin + 1);
    for (auto& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

bool MatchPrefix(const std::string& text, const char* keyword) {
    return text.size() <= std::strlen(keyword) &&
           std::strncmp(keyword, text.c_str(), text.size()) == 0;
}

bool ParseSmooth(Tcl_Interp* interp, Tcl_Obj* valueObj, SmoothMode& out) {
    const char* raw = Tcl_GetString(valueObj);
    const std::string text = TrimLower(raw ? raw : "");
    if (text.empty()) { out = SmoothMode::None; return true; }
    if (MatchPrefix(text, "bezier")) { out = SmoothMode::Bezier; return true; }
    if (MatchPrefix(text, "true")) { out = SmoothMode::Bezier; return true; }
    if (MatchPrefix(text, "raw")) { out = SmoothMode::Raw; return true; }
    int b = 0;
    if (Tcl_GetBoolean(interp, raw, &b) == TCL_OK) {
        out = b ? SmoothMode::Bezier : SmoothMode::None;
        return true;
    }
    Tcl_SetObjResult(interp, Tcl_ObjPrintf("bad -smooth \"%s\": expected boolean value, \"true\", \"bezier\", or \"raw\"", raw));
    return false;
}

std::string SmoothToString(SmoothMode mode) {
    switch (mode) {
        case SmoothMode::None: return "0";
        case SmoothMode::Bezier: return "true";
        case SmoothMode::Raw: return "raw";
    }
    return "0";
}

std::string CapStyleToString(CapStyle style) {
    switch (style) {
        case CapStyle::Flat: return "butt";
        case CapStyle::Square: return "projecting";
        case CapStyle::Round: return "round";
    }
    return "butt";
}

std::string JoinStyleToString(JoinStyle style) {
    switch (style) {
        case JoinStyle::Bevel: return "bevel";
        case JoinStyle::Miter: return "miter";
        case JoinStyle::Round: return "round";
    }
    return "round";
}

std::string DoubleToString(double value) {
    if (value == 0.0) return "0";  // normalizes -0
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.17g", value);
    return buf;
}

std::string DashToString(const DashPattern& pattern) {
    std::string out;
    for (std::size_t i = 0; i < pattern.dashes.size(); ++i) {
        if (i != 0) out += ' ';
        out += DoubleToString(pattern.dashes[i]);
    }
    return out;
}

std::string TagsToString(const CanvasItem& item) {
    std::string out;
    const auto& tags = item.Tags();
    for (std::size_t i = 0; i < tags.size(); ++i) {
        if (i != 0) out += ' ';
        out += tags[i];
    }
    return out;
}

// Normalizes "-fill" / "fill" to "fill"; nullptr when not an option-shaped token.
std::string NormalizeOptionName(const char* option) {
    if (!option) return {};
    const char* p = option;
    if (*p == '-') ++p;
    std::string name = p;
    for (auto& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return name;
}

}  // namespace

bool ParseItemConfig(Tcl_Interp* interp, CanvasWidget& widget,
                     int firstArg, int objc, Tcl_Obj* const objv[],
                     ItemConfig& out) {
    out = ItemConfig{};

    for (int i = firstArg; i < objc; ) {
        const char* option = Tcl_GetString(objv[i]);
        const std::string name = NormalizeOptionName(option);

        if (name.empty() || option[0] != '-') {
            Tcl_SetObjResult(interp,
                             Tcl_ObjPrintf("expected option but got \"%s\"", option ? option : ""));
            return false;
        }
        if (i + 1 >= objc) {
            Tcl_SetObjResult(interp, Tcl_ObjPrintf("missing value for option \"%s\"", option));
            return false;
        }
        Tcl_Obj* value = objv[i + 1];

        if (name == "fill") {
            out.hasFill = true;
            if (!ParseColorOption(interp, widget, value, out.fill)) return false;
        } else if (name == "outline") {
            out.hasOutline = true;
            if (!ParseColorOption(interp, widget, value, out.outline)) return false;
        } else if (name == "width") {
            double width = 0.0;
            if (Tcl_GetDoubleFromObj(interp, value, &width) != TCL_OK) {
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("bad width \"%s\": expected a number", Tcl_GetString(value)));
                return false;
            }
            if (!std::isfinite(width) || width < 0.0) {
                Tcl_SetObjResult(interp, Tcl_NewStringObj("width must be a non-negative number", -1));
                return false;
            }
            out.hasWidth = true;
            out.width = width;
        } else if (name == "dash") {
            out.hasDash = true;
            out.dash = ParseDash(interp, value);
        } else if (name == "joinstyle") {
            JoinStyle join = JoinStyle::Round;
            if (!ParseJoinStyle(interp, value, join)) return false;
            out.hasJoinStyle = true;
            out.joinStyle = join;
        } else if (name == "capstyle") {
            CapStyle cap = CapStyle::Flat;
            if (!ParseCapStyle(interp, value, cap)) return false;
            out.hasCapStyle = true;
            out.capStyle = cap;
        } else if (name == "start" || name == "extent") {
            double angle = 0.0;
            if (Tcl_GetDoubleFromObj(interp, value, &angle) != TCL_OK || !std::isfinite(angle)) {
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("bad %s \"%s\": expected a number", name.c_str(), Tcl_GetString(value)));
                return false;
            }
            if (name == "start") {
                out.hasStart = true;
                out.start = angle;
            } else {
                out.hasExtent = true;
                out.extent = angle;
            }
        } else if (name == "smooth") {
            SmoothMode smooth = SmoothMode::None;
            if (!ParseSmooth(interp, value, smooth)) return false;
            out.hasSmooth = true;
            out.smooth = smooth;
        } else if (name == "splinesteps") {
            int steps = kDefaultSplineSteps;
            // Tk parses this as an integer and clamps to [1, 100] silently.
            if (Tcl_GetIntFromObj(interp, value, &steps) != TCL_OK) {
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("bad -splinesteps \"%s\": expected an integer", Tcl_GetString(value)));
                return false;
            }
            out.hasSplineSteps = true;
            out.splineSteps = ClampSplineSteps(steps);
        } else if (name == "dashoffset") {
            double offset = 0.0;
            if (Tcl_GetDoubleFromObj(interp, value, &offset) != TCL_OK || !std::isfinite(offset)) {
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("bad dashoffset \"%s\": expected a number", Tcl_GetString(value)));
                return false;
            }
            if (offset < 0.0) {
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("bad screen distance \"%s\": expected a non-negative number", Tcl_GetString(value)));
                return false;
            }
            out.hasDashOffset = true;
            out.dashOffset = offset;
        } else if (name == "style") {
            ArcStyle style;
            const std::string raw = Tcl_GetString(value);
            if (!ArcItem::StyleFromString(raw, style)) {
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("bad -style \"%s\": expected arc, chord or pieslice", raw.c_str()));
                return false;
            }
            out.hasStyle = true;
            out.style = ArcItem::StyleName(style);
        } else if (name == "tags") {
            out.hasTags = true;
            out.tags.clear();
            int length = 0;
            if (Tcl_ListObjLength(interp, value, &length) == TCL_OK) {
                bool ok = true;
                out.tags.reserve(static_cast<std::size_t>(length));
                for (int j = 0; j < length; ++j) {
                    Tcl_Obj* element = nullptr;
                    if (Tcl_ListObjIndex(interp, value, j, &element) != TCL_OK || !element) {
                        ok = false;
                        break;
                    }
                    out.tags.emplace_back(Tcl_GetString(element));
                }
                if (!ok) {
                    Tcl_SetObjResult(interp, Tcl_NewStringObj("bad -tags list", -1));
                    return false;
                }
            } else {
                // a single tag name
                Tcl_ResetResult(interp);
                out.tags.emplace_back(Tcl_GetString(value));
            }
        } else if (name == "angle") {
            double angle = 0.0;
            if (Tcl_GetDoubleFromObj(interp, value, &angle) != TCL_OK || !std::isfinite(angle)) {
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("bad -angle \"%s\": expected a number", Tcl_GetString(value)));
                return false;
            }
            out.hasAngle = true;
            out.angle = angle;
        } else if (name == "state") {
            ItemState state = ItemState::Normal;
            const std::string raw = Tcl_GetString(value);
            if (!ItemStateFromName(raw, state)) {
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("bad -state \"%s\": must be normal, hidden, or disabled", raw.c_str()));
                return false;
            }
            out.hasState = true;
            out.state = state;
        } else if (name == "disabledfill") {
            out.hasDisabledFill = true;
            if (!ParseColorOption(interp, widget, value, out.disabledFill)) return false;
        } else if (name == "text") {
            out.hasText = true;
            out.text = Tcl_GetString(value);
        } else if (name == "font") {
            const char* spec = Tcl_GetString(value);
            if (!spec || *spec == '\0') {
                Tcl_SetObjResult(interp, Tcl_NewStringObj("expected a non-empty font name, got \"\"", -1));
                return false;
            }
            TkFontInfo font;
            if (!ResolveTkFont(interp, widget.tkwin, spec, font)) {
                return false;
            }
            out.hasFont = true;
            out.fontSpec = spec;
            out.fontResolved = true;
            out.fontInfo = font;
        } else if (name == "anchor") {
            std::string anchor;
            if (!NormalizeAnchor(Tcl_GetString(value), anchor)) {
                const char* bad = Tcl_GetString(value);
                Tcl_SetObjResult(interp, Tcl_ObjPrintf(
                    "bad anchor \"%s\": must be n, ne, e, se, s, sw, w, nw, or center",
                    bad ? bad : ""));
                return false;
            }
            out.hasAnchor = true;
            out.anchor = std::move(anchor);
        } else if (name == "justify") {
            std::string justify;
            if (!NormalizeJustify(Tcl_GetString(value), justify)) {
                const char* bad = Tcl_GetString(value);
                Tcl_SetObjResult(interp, Tcl_ObjPrintf(
                    "bad justify \"%s\": must be left, center, or right",
                    bad ? bad : ""));
                return false;
            }
            out.hasJustify = true;
            out.justify = std::move(justify);
        } else if (name == "wrap") {
            std::string wrap;
            if (!NormalizeWrap(Tcl_GetString(value), wrap)) {
                const char* bad = Tcl_GetString(value);
                Tcl_SetObjResult(interp, Tcl_ObjPrintf(
                    "bad -wrap \"%s\": must be word, char, or none",
                    bad ? bad : ""));
                return false;
            }
            out.hasWrap = true;
            out.wrap = std::move(wrap);
        } else if (name == "image") {
            const char* imageName = Tcl_GetString(value);
            if (!imageName) imageName = "";
            if (*imageName != '\0' &&
                Tk_FindPhoto(interp, imageName) == nullptr) {
                Tcl_SetObjResult(interp, Tcl_ObjPrintf(
                    "image \"%s\" doesn't exist or is not a photo image", imageName));
                return false;
            }
            out.hasImage = true;
            out.imageName = imageName;
        } else if (name == "bitmap") {
            const char* bitmapName = Tcl_GetString(value);
            if (!bitmapName) bitmapName = "";
            out.hasBitmap = true;
            out.bitmapName = bitmapName;
            out.bitmapResolved = false;
            out.bitmapWidth = 0;
            out.bitmapHeight = 0;
            out.bitmapBits.clear();
            if (*bitmapName != '\0') {
                XbmBitmap bits;
                if (!TkBitmapToBits(interp, bitmapName, bits)) return false;
                out.bitmapResolved = true;
                out.bitmapWidth = bits.width;
                out.bitmapHeight = bits.height;
                out.bitmapBits = std::move(bits.bits);
            }
        } else if (name == "window") {
            const char* spec = Tcl_GetString(value);
            out.hasWindow = true;
            out.windowPath = spec ? spec : "";
        } else if (name == "height") {
            double height = 0.0;
            if (Tcl_GetDoubleFromObj(interp, value, &height) != TCL_OK ||
                !std::isfinite(height)) {
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("bad height \"%s\": expected a number", Tcl_GetString(value)));
                return false;
            }
            if (height < 0.0) {
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("bad screen distance \"%s\": expected a non-negative number", Tcl_GetString(value)));
                return false;
            }
            out.hasHeight = true;
            out.height = height;
        } else if (name == "foreground" || name == "background") {
            std::optional<Color> color;
            if (!ParseColorOption(interp, widget, value, color)) return false;
            if (name == "foreground") {
                out.hasForeground = true;
                out.foreground = color;
            } else {
                out.hasBackground = true;
                out.background = color;
            }
        } else {
            Tcl_SetObjResult(interp, Tcl_ObjPrintf("unknown option \"%s\"", option));
            return false;
        }
        i += 2;
    }
    return true;
}

bool CheckItemConfig(Tcl_Interp* interp, CanvasWidget& widget, const CanvasItem& item, const ItemConfig& config) {
    const std::string type = item.Type();
    const bool isShape =
        type == "rectangle" || type == "oval" || type == "line" ||
        type == "polygon" || type == "arc";

    if (config.hasFill &&
        !(isShape || type == "text")) {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("option \"-fill\" is not valid for %s items", type.c_str()));
        return false;
    }
    if (config.hasDisabledFill &&
        !(isShape || type == "text")) {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("option \"-disabledfill\" is not valid for %s items", type.c_str()));
        return false;
    }
    if ((config.hasOutline || config.hasDash || config.hasDashOffset) && !isShape) {
        const char* name = config.hasOutline ? "-outline"
                          : config.hasDash ? "-dash"
                          : "-dashoffset";
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("option \"%s\" is not valid for %s items", name, type.c_str()));
        return false;
    }
    if (config.hasWidth && !(isShape || type == "text" || type == "window")) {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("option \"-width\" is not valid for %s items", type.c_str()));
        return false;
    }
    if (config.hasHeight && type != "window") {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("option \"-height\" is not valid for %s items", type.c_str()));
        return false;
    }
    if (config.hasWindow && type != "window") {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("option \"-window\" is not valid for %s items", type.c_str()));
        return false;
    }
    if (config.hasWindow && !config.windowPath.empty()) {
        Tk_Window embedded = nullptr;
        if (!ValidateWindowItemPath(interp, widget, config.windowPath, embedded)) {
            return false;
        }
    }
    if (config.hasJoinStyle &&
        item.Type() != "polygon" && item.Type() != "line") {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("option \"-joinstyle\" is not valid for %s items", item.Type().c_str()));
        return false;
    }
    if (config.hasCapStyle && item.Type() != "line") {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("option \"-capstyle\" is not valid for %s items", item.Type().c_str()));
        return false;
    }
    if (config.hasSmooth && item.Type() != "line" && item.Type() != "polygon") {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("option \"-smooth\" is not valid for %s items", item.Type().c_str()));
        return false;
    }
    if (config.hasSplineSteps && item.Type() != "line" && item.Type() != "polygon") {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("option \"-splinesteps\" is not valid for %s items", item.Type().c_str()));
        return false;
    }

    const bool isText = type == "text";
    const bool isImage = type == "image";
    const bool isBitmap = type == "bitmap";
    const bool isWindow = type == "window";
    const bool supportsAnchor = isText || isImage || isBitmap || isWindow;
    auto reject = [&](const char* name) {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("option \"%s\" is not valid for %s items", name, type.c_str()));
        return false;
    };
    if (config.hasText && !isText) return reject("-text");
    if (config.hasFont && !isText) return reject("-font");
    if (config.hasJustify && !isText) return reject("-justify");
    if (config.hasWrap && !isText) return reject("-wrap");
    if (config.hasAngle && !isText) return reject("-angle");
    if (config.hasAnchor && !supportsAnchor) return reject("-anchor");
    if (config.hasImage && !isImage) return reject("-image");
    if (config.hasBitmap && !isBitmap) return reject("-bitmap");
    if (config.hasForeground && !isBitmap) return reject("-foreground");
    if (config.hasBackground && !isBitmap) return reject("-background");
    if (!config.hasStart && !config.hasExtent && !config.hasStyle) return true;
    if (item.Type() == "arc") return true;

    const char* name = config.hasStart ? "-start" : config.hasExtent ? "-extent" : "-style";
    Tcl_SetObjResult(interp, Tcl_ObjPrintf("option \"%s\" is not valid for %s items", name, item.Type().c_str()));
    return false;
}

namespace {

void RefreshTextLayout(CanvasWidget& widget, TextItem& item, const ItemConfig& config) {
    TkFontInfo effective;
    if (config.hasFont && config.fontResolved) {
        effective = config.fontInfo;
    } else {
        if (!ResolveTkFont(widget.interp, widget.tkwin, item.FontSpec().c_str(), effective)) {
            effective.family = item.Font().family;
            effective.size = item.Font().size;
            effective.bold = item.Font().bold;
            effective.italic = item.Font().italic;
            effective.underline = item.Font().underline;
            effective.overstrike = item.Font().overstrike;
        }
    }

    TextItem::FontStyle style;
    style.family = effective.family;
    style.size = effective.size;
    style.bold = effective.bold;
    style.italic = effective.italic;
    style.underline = effective.underline;
    style.overstrike = effective.overstrike;
    item.SetFontStyle(style);

    TkFontMetrics metrics;
    if (QueryTkFontMetrics(widget.interp, widget.tkwin, effective, metrics)) {
        item.SetLineHeight(metrics.lineSpace());
    }

    const MeasuredTextLayout measured = MeasureTextLayout(
        widget.interp, widget.tkwin, effective, item.Text(),
        item.WrapWidth(), item.WrapMode());
    TextBlockLayout layout;
    layout.lines = measured.lines;
    layout.lineWidths = measured.lineWidths;
    layout.maxLineWidth = measured.maxLineWidth;
    item.SetLayout(std::move(layout));
}

}  // namespace

void ApplyItemConfig(CanvasWidget& widget, int itemId, CanvasItem& item, const ItemConfig& config) {
    if (config.hasFill) item.Style().fill = config.fill;
    if (config.hasOutline) item.Style().outline = config.outline;
    if (config.hasDisabledFill) item.Style().disabledFill = config.disabledFill;
    if (config.hasState) {
        item.Style().state = config.state;
    }
    if (config.hasWidth) {
        if (item.Type() == "text") {
            static_cast<TextItem&>(item).SetWrapWidth(config.width);
        } else if (item.Type() == "window") {
            // -width on a window item is its pixel width, handled below.
        } else {
            item.Style().width = config.width;
        }
    }
    if (config.hasDash) item.Style().dash = config.dash;
    if (config.hasDashOffset) item.Style().dashOffset = config.dashOffset;
    if (config.hasJoinStyle) item.Style().join = config.joinStyle;
    if (config.hasCapStyle) item.Style().cap = config.capStyle;
    if (config.hasTags) {
        const std::vector<std::string> current = widget.model.TagsOf(itemId);
        for (const auto& tag : current) widget.model.RemoveTag(itemId, tag);
        for (const auto& tag : config.tags) widget.model.AddTag(itemId, tag);
    }
    if (item.Type() == "line" || item.Type() == "polygon") {
        SmoothOptions smooth;
        if (item.Type() == "line") smooth = static_cast<const LineItem&>(item).Smooth();
        else smooth = static_cast<const PolygonItem&>(item).Smooth();
        if (config.hasSmooth) smooth.mode = config.smooth;
        if (config.hasSplineSteps) smooth.splineSteps = config.splineSteps;
        if (config.hasSmooth || config.hasSplineSteps) {
            if (item.Type() == "line") static_cast<LineItem&>(item).SetSmooth(smooth);
            else static_cast<PolygonItem&>(item).SetSmooth(smooth);
        }
    }
    if (item.Type() == "arc" && (config.hasStart || config.hasExtent || config.hasStyle)) {
        auto* arc = static_cast<ArcItem*>(&item);
        if (config.hasStart && config.hasExtent) {
            arc->SetAngles(config.start, config.extent);
        } else if (config.hasStart) {
            arc->SetAngles(config.start, arc->Extent());
        } else if (config.hasExtent) {
            arc->SetAngles(arc->Start(), config.extent);
        }
        if (config.hasStyle) {
            ArcStyle style = ArcStyle::PieSlice;
            if (ArcItem::StyleFromString(config.style, style)) arc->SetStyle(style);
        }
    }
    if (item.Type() == "image") {
        auto* image = static_cast<ImageItem*>(&item);
        if (config.hasImage) {
            image->SetImageName(config.imageName);
        }
        if (config.hasAnchor) image->SetAnchor(config.anchor);
        if (config.hasImage) {
            RgbaImage rgba;
            if (!image->ImageName().empty() &&
                TkPhotoToRgba(widget.interp, widget.tkwin, image->ImageName(), rgba)) {
                image->SetPixels(std::move(rgba.pixels), rgba.width, rgba.height);
            } else {
                image->ClearPixels();
            }
        }
    }
    if (item.Type() == "bitmap") {
        auto* bitmap = static_cast<BitmapItem*>(&item);
        if (config.hasBitmap) {
            bitmap->SetBitmapName(config.bitmapName);
            if (config.bitmapResolved) {
                bitmap->SetBitmapBits(config.bitmapBits, config.bitmapWidth, config.bitmapHeight);
            } else {
                bitmap->SetBitmapBits({}, 0, 0);
            }
        }
        if (config.hasForeground) bitmap->SetForeground(config.foreground);
        if (config.hasBackground) bitmap->SetBackground(config.background);
        if (config.hasAnchor) bitmap->SetAnchor(config.anchor);
    }
    if (item.Type() == "text") {
        auto* text = static_cast<TextItem*>(&item);
        bool needsLayoutRefresh = false;
        if (config.hasText) {
            text->SetText(config.text);
            needsLayoutRefresh = true;
        }
        if (config.hasFont) {
            text->SetFontSpec(config.fontSpec);
            if (config.fontResolved) text->SetFontStyle({
                config.fontInfo.family, config.fontInfo.size, config.fontInfo.bold,
                config.fontInfo.italic, config.fontInfo.underline, config.fontInfo.overstrike});
            needsLayoutRefresh = true;
        }
        if (config.hasWrap) {
            text->SetWrapMode(config.wrap);
            needsLayoutRefresh = true;
        }
        if (config.hasWidth) {
            needsLayoutRefresh = true;
        }
        if (config.hasJustify) {
            text->SetJustify(config.justify);
        }
        if (config.hasAngle) {
            text->SetAngle(config.angle);
        }
        if (config.hasAnchor) {
            text->SetAnchor(config.anchor);
        }
        if (needsLayoutRefresh) {
            RefreshTextLayout(widget, *text, config);
        }
    }
    if (item.Type() == "window") {
        auto* w = static_cast<WindowItem*>(&item);
        if (config.hasAnchor) w->SetAnchor(config.anchor);
        if (config.hasWindow) {
            w->SetWindowPath(config.windowPath);
            AttachWindowItem(widget, itemId);
        }
        if (config.hasWidth || config.hasHeight) {
            const int rqW = config.hasWidth
                ? std::max(0, static_cast<int>(std::lround(config.width)))
                : w->RequestedWidth();
            const int rqH = config.hasHeight
                ? std::max(0, static_cast<int>(std::lround(config.height)))
                : w->RequestedHeight();
            w->SetRequestedSize(rqW, rqH);
        }
    }
}

std::string ItemOptionValueString(const CanvasItem& item, const char* option) {
    const std::string name = NormalizeOptionName(option);
    if (name == "fill") {
        const auto& fill = item.Style().fill;
        return fill ? fill->ToHex() : "";
    }
    if (name == "outline") {
        const auto& outline = item.Style().outline;
        return outline ? outline->ToHex() : "";
    }
    if (name == "width") {
        if (item.Type() == "text") {
            return DoubleToString(static_cast<const TextItem&>(item).WrapWidth());
        }
        if (item.Type() == "window") {
            return std::to_string(static_cast<const WindowItem&>(item).RequestedWidth());
        }
        return DoubleToString(item.Style().width);
    }
    if (name == "height") {
        if (item.Type() != "window") return {};
        return std::to_string(static_cast<const WindowItem&>(item).RequestedHeight());
    }
    if (name == "window") {
        if (item.Type() != "window") return {};
        return static_cast<const WindowItem&>(item).WindowPath();
    }
    if (name == "dash") {
        return DashToString(item.Style().dash);
    }
    if (name == "dashoffset") {
        return DoubleToString(item.Style().dashOffset);
    }
    if (name == "smooth") {
        if (item.Type() != "line" && item.Type() != "polygon") return {};
        const SmoothOptions& smooth = item.Type() == "line"
            ? static_cast<const LineItem&>(item).Smooth()
            : static_cast<const PolygonItem&>(item).Smooth();
        return SmoothToString(smooth.mode);
    }
    if (name == "splinesteps") {
        if (item.Type() != "line" && item.Type() != "polygon") return {};
        const SmoothOptions& smooth = item.Type() == "line"
            ? static_cast<const LineItem&>(item).Smooth()
            : static_cast<const PolygonItem&>(item).Smooth();
        return std::to_string(smooth.splineSteps);
    }

    if (name == "tags") {
        return TagsToString(item);
    }
    if (name == "joinstyle") {
        if (item.Type() != "polygon" && item.Type() != "line") return {};
        return JoinStyleToString(item.Style().join);
    }
    if (name == "capstyle") {
        if (item.Type() != "line") return {};
        return CapStyleToString(item.Style().cap);
    }
    if (name == "start" || name == "extent" || name == "style") {
        if (item.Type() != "arc") return {};
        const auto& arc = static_cast<const ArcItem&>(item);
        if (name == "start") return DoubleToString(arc.Start());
        if (name == "extent") return DoubleToString(arc.Extent());
        return ArcItem::StyleName(arc.GetStyle());
    }
    if (name == "text" || name == "font" || name == "justify" || name == "wrap") {
        if (item.Type() != "text") return {};
        const auto& text = static_cast<const TextItem&>(item);
        if (name == "text") return text.Text();
        if (name == "font") return text.FontSpec();
        if (name == "justify") return text.Justify();
        return text.WrapMode();
    }
    if (name == "angle") {
        if (item.Type() != "text") return {};
        return DoubleToString(static_cast<const TextItem&>(item).Angle());
    }
    if (name == "anchor") {
        if (item.Type() == "text") return static_cast<const TextItem&>(item).Anchor();
        if (item.Type() == "image") return static_cast<const ImageItem&>(item).Anchor();
        if (item.Type() == "bitmap") return static_cast<const BitmapItem&>(item).Anchor();
        if (item.Type() == "window") return static_cast<const WindowItem&>(item).Anchor();
        return {};
    }
    if (name == "image") {
        if (item.Type() != "image") return {};
        return static_cast<const ImageItem&>(item).ImageName();
    }
    if (name == "bitmap" || name == "foreground" || name == "background") {
        if (item.Type() != "bitmap") return {};
        const auto& bitmap = static_cast<const BitmapItem&>(item);
        if (name == "bitmap") return bitmap.BitmapName();
        if (name == "foreground") return bitmap.HasForeground() ? bitmap.Foreground().ToHex() : "";
        return bitmap.HasBackground() ? bitmap.Background().ToHex() : "";
    }
    const std::string itemType = item.Type();
    const bool isShapeType = itemType == "rectangle" || itemType == "oval" || itemType == "line" ||
                             itemType == "polygon" || itemType == "arc";
    if (name == "disabledfill") {
        if (!isShapeType && itemType != "text") return {};
        const auto& fill = item.Style().disabledFill;
        return fill ? fill->ToHex() : "";
    }
    if (name == "state") {
        return ItemStateName(item.Style().state);
    }
    return {};
}

Tcl_Obj* ItemConfigReport(Tcl_Interp* interp, CanvasItem& item) {
    Tcl_Obj* dict = Tcl_NewDictObj();
    const bool isShape =
        item.Type() == "rectangle" || item.Type() == "oval" || item.Type() == "line" ||
        item.Type() == "polygon" || item.Type() == "arc";
    if (isShape || item.Type() == "text") {
        Tcl_Obj* value = Tcl_NewStringObj(ItemOptionValueString(item, "-width").c_str(), -1);
        Tcl_DictObjPut(interp, dict, Tcl_NewStringObj("-width", -1), value);
    }
    if (isShape || item.Type() == "text") {
        Tcl_Obj* value = Tcl_NewStringObj(ItemOptionValueString(item, "-fill").c_str(), -1);
        Tcl_DictObjPut(interp, dict, Tcl_NewStringObj("-fill", -1), value);
    }
    if (isShape) {
        for (const char* name : {"-outline", "-dash", "-dashoffset", "-tags"}) {
            Tcl_Obj* value = Tcl_NewStringObj(ItemOptionValueString(item, name).c_str(), -1);
            Tcl_DictObjPut(interp, dict, Tcl_NewStringObj(name, -1), value);
        }
    } else {
        Tcl_Obj* value = Tcl_NewStringObj(ItemOptionValueString(item, "-tags").c_str(), -1);
        Tcl_DictObjPut(interp, dict, Tcl_NewStringObj("-tags", -1), value);
    }
    if (item.Type() == "line" || item.Type() == "polygon") {
        for (const char* name : {"-joinstyle", "-smooth", "-splinesteps"}) {
            Tcl_Obj* value = Tcl_NewStringObj(ItemOptionValueString(item, name).c_str(), -1);
            Tcl_DictObjPut(interp, dict, Tcl_NewStringObj(name, -1), value);
        }
    }
    if (item.Type() == "line") {
        Tcl_Obj* value = Tcl_NewStringObj(ItemOptionValueString(item, "-capstyle").c_str(), -1);
        Tcl_DictObjPut(interp, dict, Tcl_NewStringObj("-capstyle", -1), value);
    }
    if (item.Type() == "arc") {
        for (const char* name : {"-start", "-extent", "-style"}) {
            Tcl_Obj* value = Tcl_NewStringObj(ItemOptionValueString(item, name).c_str(), -1);
            Tcl_DictObjPut(interp, dict, Tcl_NewStringObj(name, -1), value);
        }
    }
    if (item.Type() == "text") {
        for (const char* name : {"-text", "-font", "-anchor", "-justify", "-wrap", "-angle"}) {
            Tcl_Obj* value = Tcl_NewStringObj(ItemOptionValueString(item, name).c_str(), -1);
            Tcl_DictObjPut(interp, dict, Tcl_NewStringObj(name, -1), value);
        }
    }
    if (item.Type() == "image") {
        for (const char* name : {"-image", "-anchor"}) {
            Tcl_Obj* value = Tcl_NewStringObj(ItemOptionValueString(item, name).c_str(), -1);
            Tcl_DictObjPut(interp, dict, Tcl_NewStringObj(name, -1), value);
        }
    }
    if (item.Type() == "bitmap") {
        for (const char* name : {"-bitmap", "-foreground", "-background", "-anchor"}) {
            Tcl_Obj* value = Tcl_NewStringObj(ItemOptionValueString(item, name).c_str(), -1);
            Tcl_DictObjPut(interp, dict, Tcl_NewStringObj(name, -1), value);
        }
    }
    if (item.Type() == "window") {
        for (const char* name : {"-window", "-width", "-height", "-anchor"}) {
            Tcl_Obj* value = Tcl_NewStringObj(ItemOptionValueString(item, name).c_str(), -1);
            Tcl_DictObjPut(interp, dict, Tcl_NewStringObj(name, -1), value);
        }
    }
    if (isShape || item.Type() == "text") {
        Tcl_Obj* value = Tcl_NewStringObj(ItemOptionValueString(item, "-disabledfill").c_str(), -1);
        Tcl_DictObjPut(interp, dict, Tcl_NewStringObj("-disabledfill", -1), value);
    }
    {
        Tcl_Obj* value = Tcl_NewStringObj(ItemOptionValueString(item, "-state").c_str(), -1);
        Tcl_DictObjPut(interp, dict, Tcl_NewStringObj("-state", -1), value);
    }
    return dict;
}

}  // namespace tkd2dcanvas
