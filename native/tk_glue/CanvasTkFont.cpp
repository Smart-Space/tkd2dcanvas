#include "CanvasTkFont.h"

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace tkd2dcanvas {

namespace {
Tcl_Obj* DictGet(Tcl_Interp* interp, Tcl_Obj* dict, const char* key) {
    Tcl_Obj* value = nullptr;
    if (Tcl_DictObjGet(interp, dict, Tcl_NewStringObj(key, -1), &value) != TCL_OK) {
        return nullptr;
    }
    return value;
}

double PixelsPerPoint(Tcl_Interp* interp) {
    Tcl_Obj* args[] = {
        Tcl_NewStringObj("tk", -1),
        Tcl_NewStringObj("scaling", -1),
    };
    if (Tcl_EvalObjv(interp, static_cast<int>(sizeof args / sizeof args[0]), args, 0) == TCL_OK) {
        double value = 0.0;
        if (Tcl_GetDoubleFromObj(nullptr, Tcl_GetObjResult(interp), &value) == TCL_OK &&
            value > 0.0) {
            return value;
        }
    }
    return 96.0 / 72.0;
}

bool SpecRawSize(Tcl_Interp* interp, const char* spec, double& sizeOut) {
    Tcl_Obj* listObj = Tcl_NewStringObj(spec, -1);
    int length = 0;
    if (Tcl_ListObjLength(interp, listObj, &length) != TCL_OK) {
        Tcl_DecrRefCount(listObj);
        Tcl_ResetResult(interp);
        return false;
    }
    for (int i = 0; i < length; ++i) {
        Tcl_Obj* element = nullptr;
        if (Tcl_ListObjIndex(interp, listObj, i, &element) != TCL_OK || !element) break;
        double value = 0.0;
        if (Tcl_GetDoubleFromObj(nullptr, element, &value) == TCL_OK) {
            sizeOut = value;
            Tcl_DecrRefCount(listObj);
            return true;
        }
    }
    Tcl_DecrRefCount(listObj);
    return false;
}
}  // namespace

// Build the Tcl list spec for the exact pixel-size face the renderer draws:
//   {family} <-pixels> ?bold? ?italic? ?underline? ?overstrike?
// Tk spec word order: family, then size, then style words.
Tcl_Obj* BuildTkFontSpec(TkFontInfo const& font) {
    const int pixels = static_cast<int>(std::lround(std::fabs(font.size)));
    const std::string sizeText = "-" + std::to_string(pixels > 0 ? pixels : 1);

    Tcl_Obj* spec = Tcl_NewListObj(0, nullptr);
    Tcl_ListObjAppendElement(nullptr, spec, Tcl_NewStringObj(font.family.c_str(), -1));
    Tcl_ListObjAppendElement(nullptr, spec, Tcl_NewStringObj(sizeText.c_str(), -1));
    if (font.bold) Tcl_ListObjAppendElement(nullptr, spec, Tcl_NewStringObj("bold", -1));
    if (font.italic) Tcl_ListObjAppendElement(nullptr, spec, Tcl_NewStringObj("italic", -1));
    if (font.underline) Tcl_ListObjAppendElement(nullptr, spec, Tcl_NewStringObj("underline", -1));
    if (font.overstrike) Tcl_ListObjAppendElement(nullptr, spec, Tcl_NewStringObj("overstrike", -1));
    return spec;
}

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
        Tcl_NewStringObj(spec, -1),
        Tcl_NewStringObj("-displayof", -1),
        Tcl_NewStringObj(winPath, -1),
    };
    const int code = Tcl_EvalObjv(interp, static_cast<int>(sizeof args / sizeof args[0]), args, 0);
    if (code != TCL_OK) {
        return false;  // Tcl error message already on interp
    }

    Tcl_Obj* dict = Tcl_GetObjResult(interp);
    Tcl_IncrRefCount(dict);

    const double pixelsPerPoint = PixelsPerPoint(interp);
    double rawSize = 0.0;
    const bool hasRawSize = SpecRawSize(interp, spec, rawSize);

    if (Tcl_Obj* family = DictGet(interp, dict, "-family")) {
        out.family = Tcl_GetString(family);
    }
    if (hasRawSize && rawSize < 0.0) {
        out.size = -rawSize;
    } else if (hasRawSize && rawSize > 0.0) {
        out.size = static_cast<double>(static_cast<int>(rawSize * pixelsPerPoint + 0.5));  // Tk: (int)(px + 0.5)
    } else if (Tcl_Obj* size = DictGet(interp, dict, "-size")) {
        double s = 0.0;
        if (Tcl_GetDoubleFromObj(nullptr, size, &s) == TCL_OK) {
            out.size = static_cast<double>(static_cast<int>(std::fabs(s) * pixelsPerPoint + 0.5));
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

    Tcl_DecrRefCount(dict);
    return true;
}

bool QueryTkFontMetrics(Tcl_Interp* interp, Tk_Window tkwin, const TkFontInfo& font, TkFontMetrics& out)
{
    if (!tkwin) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("no Tk display for font metrics", -1));
        return false;
    }

    Tcl_Obj* spec = BuildTkFontSpec(font);

    const char* winPath = Tk_PathName(tkwin);
    Tcl_Obj* args[] = {
        Tcl_NewStringObj("font", -1),
        Tcl_NewStringObj("metrics", -1),
        spec,
        Tcl_NewStringObj("-displayof", -1),
        Tcl_NewStringObj(winPath, -1),
    };
    if (Tcl_EvalObjv(interp, static_cast<int>(sizeof args / sizeof args[0]), args, 0) != TCL_OK) {
        Tcl_DecrRefCount(spec);
        return false;  // Tcl error message already on interp
    }
    Tcl_DecrRefCount(spec);

    Tcl_Obj* dict = Tcl_GetObjResult(interp);
    bool any = false;
    if (Tcl_Obj* ascent = DictGet(interp, dict, "-ascent")) {
        int value = 0;
        if (Tcl_GetIntFromObj(nullptr, ascent, &value) == TCL_OK) {
            out.ascent = static_cast<double>(value);
            any = true;
        }
    }
    if (Tcl_Obj* descent = DictGet(interp, dict, "-descent")) {
        int value = 0;
        if (Tcl_GetIntFromObj(nullptr, descent, &value) == TCL_OK) {
            out.descent = static_cast<double>(value);
            any = true;
        }
    }
    if (!any) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("font metrics: no -ascent/-descent in result", -1));
        return false;
    }
    return true;
}

bool MeasureTkFont(Tcl_Interp* interp, Tk_Window tkwin, const TkFontInfo& font, const std::string& utf8, int& width)
{
    if (!tkwin) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("no Tk display for font measure", -1));
        return false;
    }

    Tcl_Obj* spec = BuildTkFontSpec(font);

    const char* winPath = Tk_PathName(tkwin);
    Tcl_Obj* args[] = {
        Tcl_NewStringObj("font", -1),
        Tcl_NewStringObj("measure", -1),
        spec,
        Tcl_NewStringObj("-displayof", -1),
        Tcl_NewStringObj(winPath, -1),
        Tcl_NewStringObj(utf8.c_str(), static_cast<int>(utf8.size())),
    };
    if (Tcl_EvalObjv(interp, static_cast<int>(sizeof args / sizeof args[0]), args, 0) != TCL_OK) {
        Tcl_DecrRefCount(spec);
        return false;  // Tcl error message already on interp
    }
    Tcl_DecrRefCount(spec);

    int value = 0;
    if (Tcl_GetIntFromObj(interp, Tcl_GetObjResult(interp), &value) != TCL_OK) {
        return false;
    }
    width = value;
    return true;
}

}  // namespace tkd2dcanvas
