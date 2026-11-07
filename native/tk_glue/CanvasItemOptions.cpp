#include "CanvasItemOptions.h"

#include <cmath>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include "tkd2dcanvas/CanvasItem.h"
#include "tkd2dcanvas/Color.h"
#include "tkd2dcanvas/DashPattern.h"

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

bool ApplyColorOption(Tcl_Interp* interp, CanvasWidget& widget,
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

}  // namespace

bool ApplyItemOption(Tcl_Interp* interp, CanvasWidget& widget, int itemId,
                     CanvasItem& item, const char* option, Tcl_Obj* valueObj) {
    if (!option || option[0] != '-') {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("expected option but got \"%s\"",
                                               option ? option : ""));
        return false;
    }

    if (std::strcmp(option, "-fill") == 0) {
        return ApplyColorOption(interp, widget, valueObj, item.Style().fill);
    }
    if (std::strcmp(option, "-outline") == 0) {
        return ApplyColorOption(interp, widget, valueObj, item.Style().outline);
    }
    if (std::strcmp(option, "-width") == 0) {
        double width = 0.0;
        if (Tcl_GetDoubleFromObj(interp, valueObj, &width) != TCL_OK) {
            Tcl_SetObjResult(interp, Tcl_ObjPrintf("bad width \"%s\": expected a number",
                                                   Tcl_GetString(valueObj)));
            return false;
        }
        if (!std::isfinite(width) || width < 0.0) {
            Tcl_SetObjResult(interp, Tcl_NewStringObj("width must be a non-negative number", -1));
            return false;
        }
        item.Style().width = width;
        return true;
    }
    if (std::strcmp(option, "-dash") == 0) {
        item.Style().dash = ParseDash(interp, valueObj);
        return true;
    }
    if (std::strcmp(option, "-tags") == 0) {
        // Accepts a single tag or a Tcl list of tags.
        Tcl_Obj* listObj = valueObj;
        int length = 0;
        if (Tcl_ListObjLength(interp, listObj, &length) == TCL_OK) {
            bool ok = true;
            std::vector<std::string> tags;
            tags.reserve(static_cast<std::size_t>(length));
            for (int i = 0; i < length; ++i) {
                Tcl_Obj* element = nullptr;
                if (Tcl_ListObjIndex(interp, listObj, i, &element) != TCL_OK || !element) {
                    ok = false;
                    break;
                }
                tags.emplace_back(Tcl_GetString(element));
            }
            if (!ok) {
                Tcl_SetObjResult(interp, Tcl_NewStringObj("bad -tags list", -1));
                return false;
            }
            for (const auto& tag : tags) widget.model.AddTag(itemId, tag);
            return true;
        }
        Tcl_ResetResult(interp);
        widget.model.AddTag(itemId, Tcl_GetString(valueObj));
        return true;
    }

    Tcl_SetObjResult(interp, Tcl_ObjPrintf("unknown option \"%s\"", option));
    return false;
}

}  // namespace tkd2dcanvas
