// CanvasItemCommand.h — dispatcher for the per-widget command
//   .c create rectangle ...
//   .c coords item ?coords?
//   .c move tagOrId dx dy
//   .c delete tagOrId ...
//   .c itemconfigure item ?-option value? ...
//   .c bbox tagOrId ...
#pragma once

#include <tcl.h>

namespace tkd2dcanvas {

struct CanvasWidget;

// Handles the item related subcommands of a canvas widget command.
// `objv[0]` is the widget command itself, `objv[1]` the subcommand.
int CanvasItemSubcommand(CanvasWidget& widget, Tcl_Interp* interp, int objc, Tcl_Obj* const objv[]);

}  // namespace tkd2dcanvas
