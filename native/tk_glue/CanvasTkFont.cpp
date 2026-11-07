#include "CanvasTkFont.h"

#include <cmath>
#include <cstring>

namespace tkd2dcanvas {

namespace {
Tcl_Obj* DictGet(Tcl_Interp* interp, Tcl_Obj* dict, const char* key) {
    Tcl_Obj* value = nullptr;
    if (Tcl_DictObjGet(interp, dict, Tcl_NewStringObj(key, -1), &value) != TCL_OK) {
        return nullptr;
    }
    return value;
}
}  // namespace

bool ResolveTkFont(Tcl_Interp* interp, Tk_Window tkwin, const char* spec, TkFontInfo& out)
{
    if (!tkwin || !spec || *spec == '\0') {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("invalid font request", -1));
        return false;
    }

    const char* winPath = Tk_PathName(tkwin);
    Tcl_Obj* args[] = {
        Tcl_NewStringObj("font", -1),
        Tcl_NewStringObj("actual", -1),
        Tcl_NewStringObj("-displayof", -1),
        Tcl_NewStringObj(winPath, -1),
        Tcl_NewStringObj("-font", -1),
        Tcl_NewStringObj(spec, -1),
    };
    const int code = Tcl_EvalObjv(interp, static_cast<int>(sizeof args / sizeof args[0]), args, 0);
    if (code != TCL_OK) {
        return false;  // Tcl error message already on interp
    }

    Tcl_Obj* dict = Tcl_GetObjResult(interp);

    if (Tcl_Obj* family = DictGet(interp, dict, "-family")) {
        out.family = Tcl_GetString(family);
    }
    if (Tcl_Obj* size = DictGet(interp, dict, "-size")) {
        int s = 0;
        if (Tcl_GetIntFromObj(nullptr, size, &s) == TCL_OK) {
            // Tk uses negative sizes for pixels; store absolute point-ish size.
            out.size = std::fabs(static_cast<double>(s));
        }
    }
    if (Tcl_Obj* weight = DictGet(interp, dict, "-weight")) {
        out.bold = std::strcmp(Tcl_GetString(weight), "bold") == 0;
    }
    if (Tcl_Obj* slant = DictGet(interp, dict, "-slant")) {
        out.italic = std::strcmp(Tcl_GetString(slant), "italic") == 0;
    }
    auto getBool = [&](const char* key, bool& outFlag) {
        if (Tcl_Obj* v = DictGet(interp, dict, key)) {
            int flag = 0;
            if (Tcl_GetBooleanFromObj(nullptr, v, &flag) == TCL_OK) outFlag = flag != 0;
        }
    };
    getBool("-underline", out.underline);
    getBool("-overstrike", out.overstrike);

    return true;
}

}  // namespace tkd2dcanvas
