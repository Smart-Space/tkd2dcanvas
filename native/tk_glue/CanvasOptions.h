// CanvasOptions.h — widget level configuration of the canvas widget.
//
// The options are handled by Tk's own option machinery (Tk_OptionTable +
// Tk_InitOptions/Tk_SetOptions), so `configure`/`cget` behave like every
// other Tk widget and the option database works as usual.
#pragma once

#include <tcl.h>
#include <tk.h>

namespace tkd2dcanvas {

struct CanvasWidget;

// Option change masks (the "typeMask" column of the option specs).
enum CanvasOptionMask {
    kCanvasOptGeometry = 1 << 0,   // -width/-height/-borderwidth
    kCanvasOptColour = 1 << 1,     // -background
    kCanvasOptScroll = 1 << 2,     // -scrollregion/-*scrollcommand
    kCanvasOptOther = 1 << 3,      // everything else
};

// Creates the option table for the canvas widget class.
Tk_OptionTable CreateCanvasOptionTable(Tcl_Interp* interp);

// Applies the option values that changed (mask) to the widget.
void ApplyCanvasOptions(CanvasWidget& widget, int mask);

// Builds the dictionary returned by `configure` without arguments.
int CanvasReportOptions(Tcl_Interp* interp, CanvasWidget& widget);

}  // namespace tkd2dcanvas
