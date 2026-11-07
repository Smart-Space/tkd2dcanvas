// CanvasItemCommand.h — dispatcher for the per-widget command
//   .c create rectangle ...
//   .c coords item ?coords?
//   .c move tagOrId dx dy
//   .c moveto tagOrId x y
//   .c delete tagOrId ...
//   .c itemconfigure item ?-option value? ...
//   .c bbox tagOrId ...
#pragma once

#include <tcl.h>

#include <vector>

namespace tkd2dcanvas {

struct CanvasWidget;

// Handles the item related subcommands of a canvas widget command.
// `objv[0]` is the widget command itself, `objv[1]` the subcommand.
int CanvasItemSubcommand(CanvasWidget& widget, Tcl_Interp* interp, int objc, Tcl_Obj* const objv[]);

// Resolves Tk-style item specs (item id / "all" / tag name) in
// objv[firstArg, limit) to a deduplicated list of existing item ids, sorted
// bottom-to-top (display list order).
// A purely numeric spec that does not name an existing item selects nothing
// (Tk semantics: silent no-op); a tag matching nothing is *not* an error
// either.
bool ResolveItemSpecs(CanvasWidget& widget, Tcl_Interp* interp,
                      Tcl_Obj* const objv[], int firstArg, int limit,
                      std::vector<int>& out);

}  // namespace tkd2dcanvas
