#include "CanvasItemCommand.h"

#include <cstring>
#include <string>

#include "CanvasWidget.h"
#include "CanvasScrollBridge.h"

namespace tkd2dcanvas {

namespace {
const char* Subcommand(int objc, Tcl_Obj* const objv[]) {
    return objc >= 2 ? Tcl_GetString(objv[1]) : "";
}

int NotImplemented(Tcl_Interp* interp, const char* what, const char* milestone) {
    Tcl_SetObjResult(interp, Tcl_ObjPrintf("tkd2dcanvas: %s is not implemented yet (%s)", what, milestone));
    return TCL_ERROR;
}
}  // namespace

int Tkd2dcanvasWidgetCmd(ClientData clientData,
                             Tcl_Interp* interp,
                             int objc,
                             Tcl_Obj* const objv[])
{
    auto* widget = static_cast<CanvasWidget*>(clientData);
    if (!widget) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("unknown canvas widget", -1));
        return TCL_ERROR;
    }

    const std::string cmd = Subcommand(objc, objv);

    if (cmd == "create") {
        // TODO(M4): resolve item type (rectangle/oval/line/polygon/text/image/
        // bitmap/window), parse coords, build item options, insert into model,
        // invalidate region, return new item id.
        return NotImplemented(interp, "create", "milestone M4");
    }
    if (cmd == "coords") {
        return NotImplemented(interp, "coords", "milestone M5");
    }
    if (cmd == "move") {
        return NotImplemented(interp, "move", "milestone M5");
    }
    if (cmd == "delete") {
        return NotImplemented(interp, "delete", "milestone M5");
    }
    if (cmd == "itemconfigure" || cmd == "itemcget") {
        return NotImplemented(interp, "itemconfigure/itemcget", "milestone M5");
    }
    if (cmd == "bbox") {
        return NotImplemented(interp, "bbox", "milestone M5");
    }
    if (cmd == "addtag" || cmd == "dtag") {
        return NotImplemented(interp, "(d)addtag", "milestone M6");
    }
    if (cmd == "find" || cmd == "findwithtag" || cmd == "findall" || cmd == "findclosest") {
        return NotImplemented(interp, "find withtag/all/closest", "milestone M6");
    }
    if (cmd == "bind" || cmd == "tagbind" || cmd == "current") {
        return NotImplemented(interp, "bind/tagbind", "milestone M6");
    }
    if (cmd == "xview" || cmd == "yview") {
        return HandleScrollCommand(*widget, cmd == "xview", objc - 1, objv + 1);
    }
    if (cmd == "configure" || cmd == "cget") {
        // TODO(M5): widget-level configure/cget via Tk_ConfigureWidget-style handling.
        return NotImplemented(interp, "configure/cget", "milestone M2/M5");
    }

    Tcl_SetObjResult(interp, Tcl_ObjPrintf("unknown widget subcommand \"%s\"", cmd.c_str()));
    return TCL_ERROR;
}

}  // namespace tkd2dcanvas
