#include "CanvasItemOptions.h"

#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include "tkd2dcanvas/CanvasItem.h"

#include "ArcItem.h"
#include "LineItem.h"
#include "PolygonItem.h"
#include "CanvasTkColor.h"
#include "CanvasWidget.h"

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
        } else {
            Tcl_SetObjResult(interp, Tcl_ObjPrintf("unknown option \"%s\"", option));
            return false;
        }
        i += 2;
    }
    return true;
}

bool CheckItemConfig(Tcl_Interp* interp, const CanvasItem& item,
                     const ItemConfig& config) {
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
    if (!config.hasStart && !config.hasExtent && !config.hasStyle) return true;
    if (item.Type() == "arc") return true;

    const char* name = config.hasStart ? "-start" : config.hasExtent ? "-extent" : "-style";
    Tcl_SetObjResult(interp, Tcl_ObjPrintf("option \"%s\" is not valid for %s items", name, item.Type().c_str()));
    return false;
}

void ApplyItemConfig(CanvasWidget& widget, int itemId, CanvasItem& item, const ItemConfig& config) {
    if (config.hasFill) item.Style().fill = config.fill;
    if (config.hasOutline) item.Style().outline = config.outline;
    if (config.hasWidth) item.Style().width = config.width;
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
        return DoubleToString(item.Style().width);
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
    return {};
}

Tcl_Obj* ItemConfigReport(Tcl_Interp* interp, CanvasItem& item) {
    Tcl_Obj* dict = Tcl_NewDictObj();
    for (const char* name : {"-fill", "-outline", "-width", "-dash", "-dashoffset", "-tags"}) {
        Tcl_Obj* value = Tcl_NewStringObj(ItemOptionValueString(item, name).c_str(), -1);
        Tcl_DictObjPut(interp, dict, Tcl_NewStringObj(name, -1), value);
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
    return dict;
}

}  // namespace tkd2dcanvas
