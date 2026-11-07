#include "CanvasTkColor.h"

#include <cstdlib>
#include <string>
#include <vector>

namespace tkd2dcanvas {

namespace {
bool ParseRgbList(const char* text, Color& out) {
    // "winfo rgb" returns "r g b" (0..255 each).
    std::vector<int> values;
    const char* p = text;
    while (*p) {
        while (*p == ' ') ++p;
        if (*p == '\0') break;
        char* end = nullptr;
        const long v = std::strtol(p, &end, 10);
        if (end == p) return false;
        values.push_back(static_cast<int>(v));
        p = end;
    }
    if (values.size() != 3) return false;

    out = Color{static_cast<std::uint8_t>(values[0]),
                static_cast<std::uint8_t>(values[1]),
                static_cast<std::uint8_t>(values[2]),
                255};
    return true;
}
}  // namespace

bool ResolveTkColor(Tcl_Interp* interp, Tk_Window tkwin, const char* spec, Color& out)
{
    if (spec == nullptr || *spec == '\0') {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("empty color specification", -1));
        return false;
    }

    // 1) Built-in parser: #rgb/#rrggbb/#rrggbbaa and a small name table.
    if (Color::TryParse(spec, out)) return true;

    if (!tkwin) {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("unknown color \"%s\"", spec));
        return false;
    }

    // 2) Full Tk resolution via `winfo rgb` (handles the whole color table,
    //    "systemButtonFace" style colors and "#rrggbb"). The result is
    //    "r g b" in 0..255.
    Tcl_Obj* args[] = {
        Tcl_NewStringObj("winfo", -1),
        Tcl_NewStringObj("rgb", -1),
        Tcl_NewStringObj(Tk_PathName(tkwin), -1),
        Tcl_NewStringObj(spec, -1),
    };
    if (Tcl_EvalObjv(interp, 4, args, TCL_EVAL_GLOBAL) != TCL_OK) {
        return false;  // Tk already set the error message
    }

    const char* rgb = Tcl_GetString(Tcl_GetObjResult(interp));
    if (!ParseRgbList(rgb, out)) {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("unknown color \"%s\"", spec));
        return false;
    }
    return true;
}

}  // namespace tkd2dcanvas
