#include "CanvasScrollBridge.h"

#include "CanvasWidget.h"

namespace tkd2dcanvas {

int HandleScrollCommand(CanvasWidget& widget, bool horizontal,
                        int objc, Tcl_Obj* const objv[]) {
    // TODO(M7): parse 0/2/3-argument forms, clamp via CanvasView::ScrollToFraction,
    // call NotifyScrollCommand() after changes, redraw.
    (void)widget;
    (void)horizontal;
    (void)objc;
    (void)objv;
    Tcl_SetObjResult(widget.interp,
                     Tcl_NewStringObj("scrolling is not implemented yet (milestone M7)", -1));
    return TCL_ERROR;
}

void NotifyScrollCommand(CanvasWidget& widget, bool horizontal) {
    // TODO(M7): eval the configured -xscrollcommand/-yscrollcommand with
    //   {first last} arguments; ignore failures but record them for the
    //   widget's error reporting (Tk semantics: errors in scroll commands
    //   are reported, not fatal).
    (void)widget;
    (void)horizontal;
}

}  // namespace tkd2dcanvas
