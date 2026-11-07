#include "CanvasItemCommand.h"

#include <cstring>
#include <string>

#include "CanvasWidget.h"

namespace tkd2dcanvas {

namespace {

int NotImplemented(Tcl_Interp* interp, const char* what, const char* milestone) {
    Tcl_SetObjResult(interp, Tcl_ObjPrintf("tkd2dcanvas: %s is not implemented yet (%s)",
                                            what, milestone));
    return TCL_ERROR;
}

}  // namespace

int CanvasItemSubcommand(CanvasWidget& widget, Tcl_Interp* interp,
                         int objc, Tcl_Obj* const objv[]) {
    const std::string cmd = (objc >= 2) ? Tcl_GetString(objv[1]) : "";

    if (cmd == "create") {
        // TODO(M4): item factory + coordinates + item options + repaint.
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
    if (cmd == "itemconfigure" || cmd == "itemconfig") {
        return NotImplemented(interp, "itemconfigure", "milestone M5");
    }
    if (cmd == "itemcget") {
        return NotImplemented(interp, "itemcget", "milestone M5");
    }
    if (cmd == "bbox") {
        return NotImplemented(interp, "bbox", "milestone M5");
    }
    if (cmd == "addtag" || cmd == "dtag" || cmd == "gettags") {
        return NotImplemented(interp, "(d)addtag/gettags", "milestone M6");
    }
    if (cmd == "find" || cmd == "findwithtag" || cmd == "findall" || cmd == "findclosest") {
        return NotImplemented(interp, "find withtag/all/closest", "milestone M6");
    }
    if (cmd == "bind" || cmd == "tagbind" || cmd == "current") {
        return NotImplemented(interp, "bind/tagbind", "milestone M6");
    }
    if (cmd == "raise" || cmd == "lower" || cmd == "tagraise" || cmd == "taglower") {
        return NotImplemented(interp, "raise/lower", "milestone M6");
    }
    if (cmd == "xview" || cmd == "yview") {
        return NotImplemented(interp, "xview/yview", "milestone M7");
    }
    if (cmd == "scan" || cmd == "scanmark" || cmd == "scandragto") {
        return NotImplemented(interp, "scan", "milestone M7");
    }

    Tcl_SetObjResult(interp, Tcl_ObjPrintf("unknown widget subcommand \"%s\"", cmd.c_str()));
    return TCL_ERROR;
}

}  // namespace tkd2dcanvas
