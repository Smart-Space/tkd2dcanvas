#include "CanvasCmd.h"

#include <cstring>
#include <string>

#include "CanvasOptions.h"
#include "CanvasWidget.h"

namespace tkd2dcanvas {

int Tkd2dcanvasCmd(ClientData /*clientData*/,
                       Tcl_Interp* interp,
                       int objc,
                       Tcl_Obj* const objv[])
{
    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName ?options?");
        return TCL_ERROR;
    }

    const char* pathName = Tcl_GetString(objv[1]);

    CanvasOptions options;
    if (!ParseWidgetOptions(interp, objc - 2, objv + 2, options)) {
        return TCL_ERROR;  // message already set on interp
    }

    CanvasWidget* widget = CanvasWidget::Create(interp, pathName, options);
    if (!widget) {
        // Create() sets the interp error message.
        return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, Tcl_NewStringObj(pathName, -1));
    return TCL_OK;
}

int RegisterCommands(Tcl_Interp* interp)
{
    Tcl_CreateObjCommand(interp,
                         "tkd2dcanvas",
                         Tkd2dcanvasCmd,
                         nullptr,   // clientData
                         nullptr);  // delete proc
    return TCL_OK;
}

}  // namespace tkd2dcanvas
