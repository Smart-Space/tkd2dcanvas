// CanvasTkColor.h — resolve Tk color specs ("red", "#ff0000", "system colors")
// into the internal Color type.
#pragma once

#include <tcl.h>
#include <tk.h>

#include "tkd2dcanvas/Color.h"

namespace tkd2dcanvas {

// Fast path: hex strings and a small built-in name table. Falls back to
// Tk's `winfo rgb` for the full color table and system colors.
// On failure returns false and sets interp error message.
bool ResolveTkColor(Tcl_Interp* interp, Tk_Window tkwin, const char* spec, Color& out);

}  // namespace tkd2dcanvas
