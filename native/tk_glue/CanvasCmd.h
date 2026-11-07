// CanvasCmd.h — registration of the `tkd2dcanvas` Tcl command and its
// per-widget subcommand dispatcher.
#pragma once

#include <tcl.h>

namespace tkd2dcanvas {

// Creates the global `tkd2dcanvas` command. Called by Tkd2dcanvas_Init.
int RegisterCommands(Tcl_Interp* interp);

// The `tkd2dcanvas .c -width 400 ...` command: creates the widget.
Tcl_ObjCmdProc Tkd2dcanvasCmd;

}  // namespace tkd2dcanvas
