// CanvasTkFont.h — resolve Tk font specs ("TkDefaultFont", {Helvetica 12 bold})
// into DirectWrite-usable font information.
#pragma once

#include <tcl.h>
#include <tk.h>

#include <string>

namespace tkd2dcanvas {

struct TkFontInfo {
    std::string family = "TkDefaultFont";
    double size = 12.0;      // points
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool overstrike = false;
};

// Uses the Tcl command `font actual -displayof <window> -font <spec>`,
// which is stable public API (unlike Tk's private font internals) and
// yields the fully resolved dictionary for the current display.
// On failure returns false and sets interp error message.
bool ResolveTkFont(Tcl_Interp* interp, Tk_Window tkwin, const char* spec, TkFontInfo& out);

}  // namespace tkd2dcanvas
