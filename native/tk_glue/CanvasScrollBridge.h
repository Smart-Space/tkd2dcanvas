// CanvasScrollBridge.h — xview/yview command handling and scrollbar sync.
//
// The semantics follow Tk's canvas (tkCanvas.c: CANV_XVIEW / CANV_YVIEW,
// ScrollFractions, CanvasUpdateScrollbars), including the deferral of the
// -xscrollcommand/-yscrollcommand notifications to idle time: the command
// is a script prefix which is evaluated with the two fraction arguments
// ("<prefix> <first> <last>"), errors are reported via bgerror, and the
// callback is coalesced and cancelled when the widget is destroyed.
#pragma once

#include <tcl.h>
#include <tk.h>

namespace tkd2dcanvas {

struct CanvasWidget;

// Implements the widget subcommands `.c xview ?args?` / `.c yview ?args?`:
//   xview                -> returns {first last}
//   xview moveto <frac>  -> scrolls to absolute fraction
//   xview scroll <n> units|pages -> scrolls by increments
// Same for yview. Returns TCL_OK/TCL_ERROR, fills interp result.
int HandleScrollCommand(CanvasWidget& widget, bool horizontal,
                        int objc, Tcl_Obj* const objv[]);

// Implements `.c scan mark x y` / `.c scan dragto x y ?gain?` (Tk canvas
// semantics, tkCanvas.c: CANV_SCAN). Returns TCL_OK/TCL_ERROR.
int HandleScanCommand(CanvasWidget& widget, int objc, Tcl_Obj* const objv[]);

// Schedules the (deferred, coalesced) -xscrollcommand/-yscrollcommand
// notification, mirroring Tk's UPDATE_SCROLLBARS flag: every call with the
// same widget coalesces into one pending idle callback.
void RequestScrollbarUpdate(CanvasWidget& widget);
// Cancels a pending notification. Called before the widget is freed.
void CancelScrollbarUpdate(CanvasWidget& widget);
// Evaluates the configured scroll command with "<first> <last>" appended
// (TCL_EVAL_GLOBAL). Errors are reported via bgerror (Tk semantics); an
// empty command prefix is a no-op.
void NotifyScrollCommand(CanvasWidget& widget, bool horizontal);

}  // namespace tkd2dcanvas
