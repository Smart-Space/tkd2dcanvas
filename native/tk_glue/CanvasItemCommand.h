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

// Widget-bound ClientData is a CanvasWidget*.
Tcl_ObjCmdProc Tkd2dcanvasWidgetCmd;

}  // namespace tkd2dcanvas
