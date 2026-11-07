// CanvasScrollBridge.h — xview/yview command handling and scrollbar sync.
#pragma once

#include <tcl.h>
#include <tk.h>

namespace tkd2dcanvas {

struct CanvasWidget;

// Implements the widget subcommands `.c xview ?args?` / `.c yview ?args?`:
//   xview                -> returns {first last}
//   xview moveto <frac>  -> scrolls to absolute fraction
//   xview scroll <n> units|pages -> scrolls by increments
//   xview (set <first> <last>)   -> used by -xscrollcommand-style feedback
// Same for yview. Returns TCL_OK/TCL_ERROR, fills interp result.
int HandleScrollCommand(CanvasWidget& widget, bool horizontal,
                        int objc, Tcl_Obj* const objv[]);

// Notifies the configured scroll command (if any) after the view offset changed.
void NotifyScrollCommand(CanvasWidget& widget, bool horizontal);

}  // namespace tkd2dcanvas
