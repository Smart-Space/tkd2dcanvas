#include "CanvasOptions.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace tkd2dcanvas {

namespace {
bool ParseInt(const char* text, int& out) {
    errno = 0;
    char* end = nullptr;
    const long v = std::strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') return false;
    out = static_cast<int>(v);
    return true;
}

bool ParseDouble(const char* text, double& out) {
    errno = 0;
    char* end = nullptr;
    const double v = std::strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0') return false;
    out = v;
    return true;
}

bool ParseBool(const char* text, bool& out) {
    if (std::strcmp(text, "1") == 0 || std::strcmp(text, "true") == 0 ||
        std::strcmp(text, "True") == 0 || std::strcmp(text, "yes") == 0 ||
        std::strcmp(text, "on") == 0) { out = true; return true; }
    if (std::strcmp(text, "0") == 0 || std::strcmp(text, "false") == 0 ||
        std::strcmp(text, "False") == 0 || std::strcmp(text, "no") == 0 ||
        std::strcmp(text, "off") == 0) { out = false; return true; }
    return false;
}

void SetUnknown(CanvasOptions& out, const std::string& name, const std::string& value) {
    out.unknown.emplace_back(name, value);
}
}  // namespace

bool ParseWidgetOptions(Tcl_Interp* interp,
                        int objc,
                        Tcl_Obj* const objv[],
                        CanvasOptions& out)
{
    for (int i = 0; i < objc; i += 2) {
        if (i + 1 >= objc) {
            Tcl_SetObjResult(interp, Tcl_NewStringObj(
                "missing value for widget option", -1));
            return false;
        }
        const std::string name = Tcl_GetString(objv[i]);
        const char* value = Tcl_GetString(objv[i + 1]);

        if (name == "-width") { if (!ParseInt(value, out.width)) goto badValue; }
        else if (name == "-height") { if (!ParseInt(value, out.height)) goto badValue; }
        else if (name == "-background" || name == "-bg") out.background = value;
        else if (name == "-cursor") out.cursor = value;
        else if (name == "-borderwidth" || name == "-bd") { if (!ParseInt(value, out.borderWidth)) goto badValue; }
        else if (name == "-highlightthickness") { if (!ParseInt(value, out.highlightThickness)) goto badValue; }
        else if (name == "-xscrollincrement") { if (!ParseInt(value, out.xScrollIncrement)) goto badValue; }
        else if (name == "-yscrollincrement") { if (!ParseInt(value, out.yScrollIncrement)) goto badValue; }
        else if (name == "-closeenough") { if (!ParseDouble(value, out.closeEnough)) goto badValue; }
        else if (name == "-confine") { if (!ParseBool(value, out.confine)) goto badValue; }
        else if (name == "-takefocus") { if (!ParseBool(value, out.takeFocus)) goto badValue; }
        else if (name == "-scrollregion") out.scrollRegion = value;
        else if (name == "-xscrollcommand") out.xScrollCommand = value;
        else if (name == "-yscrollcommand") out.yScrollCommand = value;
        else SetUnknown(out, name, value ? value : "");
        continue;

badValue:
        Tcl_SetObjResult(interp, Tcl_ObjPrintf(
            "bad value \"%s\" for widget option \"%s\"", value, name.c_str()));
        return false;
    }
    return true;
}

}  // namespace tkd2dcanvas
