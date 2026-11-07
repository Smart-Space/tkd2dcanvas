// CanvasTagCommand.h — tag and search subcommands of the canvas widget
// command:
//   .c find <searchCommand> ?args?
//   .c addtag <tag> <searchCommand> ?args?
//   .c dtag <tagOrId> ?tagToDelete?
//   .c gettags <tagOrId>
//   .c raise <tagOrId> ?aboveThis?
//   .c lower <tagOrId> ?belowThis?
#pragma once

#include <tcl.h>

namespace tkd2dcanvas {

struct CanvasWidget;

// Handles the tag/search related subcommands of a canvas widget command.
// `objv[0]` is the widget command itself, `objv[1]` the subcommand.
int CanvasTagSubcommand(CanvasWidget& widget, Tcl_Interp* interp, int objc, Tcl_Obj* const objv[]);

}  // namespace tkd2dcanvas
