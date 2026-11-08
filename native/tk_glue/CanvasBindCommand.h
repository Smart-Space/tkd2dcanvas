// CanvasBindCommand.h — the "bind" subcommand of the canvas widget command
// and the dispatch of Tk events to the tag bindings.
//
// The canvas owns its binding logic (see docs/architecture.md): the C++ core
// decides which item is under the pointer (hit test) and in which order the
// tags are consulted; Tk's binding table provides the event pattern
// matching and the evaluation of the Tcl script.
#pragma once

#include <tcl.h>
#include <tk.h>

namespace tkd2dcanvas {

struct CanvasWidget;

// Handles `.c bind tagOrId ?sequence? ?script?` (alias: "tagbind").
int CanvasBindSubcommand(CanvasWidget& widget, Tcl_Interp* interp,
                          int objc, Tcl_Obj* const objv[]);

// Handles `.c focus ?tagOrId?` (Tk canvas semantics, tkCanvas.c:
// CANV_FOCUS). Without arguments, returns the focus item's id ("" when
// none). `.c focus ""` resets the focus item to none. Otherwise the focus
// is set to the first matching item (display list order) that supports the
// insertion cursor; if none matches or none supports it, the focus item is
// left unchanged (Tk silently ignores the request).
int CanvasFocusSubcommand(CanvasWidget& widget, Tcl_Interp* interp,
                          int objc, Tcl_Obj* const objv[]);

// Dispatches a mouse/key event to the tag bindings of the widget. The item
// under the pointer must be up to date (see CanvasWidget::PickCurrentItem).
void CanvasDispatchEvent(CanvasWidget& widget, XEvent* eventPtr);

}  // namespace tkd2dcanvas
