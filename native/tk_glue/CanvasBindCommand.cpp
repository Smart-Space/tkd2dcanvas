#include "CanvasBindCommand.h"

#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "tkd2dcanvas/CanvasModel.h"

#include "CanvasWidget.h"

namespace tkd2dcanvas {

namespace {

// The binding object used for the tag "all" (the canvas as a whole).
ClientData AllBindingObject() {
    return reinterpret_cast<ClientData>(const_cast<char*>(Tk_GetUid("all")));
}

// Returns the binding object of an item. Every item gets its own key so
// that a binding can never leak to another item (the item ids are never
// reused, so the keys stay valid for the lifetime of the widget).
ClientData ItemBindingObject(CanvasWidget& widget, int itemId) {
    auto it = widget.itemBindingObjects.find(itemId);
    if (it != widget.itemBindingObjects.end()) return it->second;

    auto* key = new char[1]();
    widget.itemBindingObjects.emplace(itemId, key);
    return key;
}

// True when the spec is a plain number (an item id rather than a tag).
bool IsNumericSpec(const char* spec) {
    if (!spec || !*spec) return false;
    for (const char* p = spec; *p; ++p) {
        if (*p < '0' || *p > '9') return false;
    }
    return true;
}

// True when the event pattern is a virtual event ("<<Foo>>"). Tk's canvas
// accepts such a pattern but never fires it, because a virtual event is
// delivered to a window and not to a canvas tag.
//
// The pattern is inspected through a plain string: touching the caller's
// Tcl_Obj (Tcl_ListObjLength and friends) would change the object's
// representation and with it the behaviour of the caller.
bool IsVirtualPattern(Tcl_Interp* interp, const char* sequence) {
    if (!sequence) return false;

    int count = 0;
    const char** elements = nullptr;
    if (Tcl_SplitList(interp, sequence, &count, &elements) != TCL_OK || !elements) {
        Tcl_ResetResult(interp);
        return false;
    }

    bool virtualEvent = false;
    for (int i = 0; i < count; ++i) {
        if (elements[i][0] == '<' && elements[i][1] == '<') {
            virtualEvent = true;
            break;
        }
    }
    Tcl_Free(reinterpret_cast<char*>(elements));
    return virtualEvent;
}

// The event types Tk's canvas allows for a tag binding: key, button, motion,
// enter, leave and virtual events.
constexpr unsigned long kAllowedEventMask =
    ButtonMotionMask | Button1MotionMask | Button2MotionMask | Button3MotionMask |
    Button4MotionMask | Button5MotionMask | ButtonPressMask | ButtonReleaseMask |
    EnterWindowMask | LeaveWindowMask | KeyPressMask | KeyReleaseMask |
    PointerMotionMask | VirtualEventMask;

}  // namespace

int CanvasBindSubcommand(CanvasWidget& widget, Tcl_Interp* interp,
                          int objc, Tcl_Obj* const objv[]) {
    if (objc < 3 || objc > 5) {
        Tcl_WrongNumArgs(interp, 2, objv, "tagOrId ?sequence? ?script?");
        return TCL_ERROR;
    }

    // The binding table is created lazily, exactly like Tk's canvas does.
    if (!widget.bindingTable) {
        widget.bindingTable = Tk_CreateBindingTable(interp);
        if (!widget.bindingTable) {
            Tcl_SetObjResult(interp, Tcl_NewStringObj("could not create binding table", -1));
            return TCL_ERROR;
        }
    }

    const char* spec = Tcl_GetString(objv[2]);
    ClientData object = nullptr;
    if (IsNumericSpec(spec)) {
        long value = 0;
        if (Tcl_GetLongFromObj(nullptr, objv[2], &value) != TCL_OK || value < 0 ||
            value > 0x7fffffffL || !widget.model.Exists(static_cast<int>(value))) {
            Tcl_SetObjResult(interp, Tcl_ObjPrintf("invalid item identifier \"%s\"", spec));
            return TCL_ERROR;
        }
        object = ItemBindingObject(widget, static_cast<int>(value));
    } else {
        object = reinterpret_cast<ClientData>(const_cast<char*>(Tk_GetUid(spec)));
    }

    if (objc == 5) {
        const char* script = Tcl_GetString(objv[4]);
        const std::string sequence = Tcl_GetString(objv[3]);

        if (IsVirtualPattern(interp, sequence.c_str())) {
            // Tk's canvas stores a virtual event pattern but never fires it
            // (a virtual event is delivered to a window, not to a tag).
            if (!script || !*script) {
                widget.ForgetVirtualBinding(object, sequence);
            } else {
                widget.RememberVirtualBinding(object, sequence, script);
            }
            Tcl_SetObjResult(interp, Tcl_NewStringObj("", -1));
            return TCL_OK;
        }

        if (!script || !*script) {
            // An empty script removes the binding.
            if (Tk_DeleteBinding(interp, widget.bindingTable, object, sequence.c_str()) != TCL_OK) {
                return TCL_ERROR;
            }
            widget.ForgetBinding(object, sequence);
            Tcl_SetObjResult(interp, Tcl_NewStringObj("", -1));
            return TCL_OK;
        }

        int append = 0;
        if (script[0] == '+') {
            ++script;
            append = 1;
        }
        const unsigned long mask = Tk_CreateBinding(interp, widget.bindingTable, object,
                                                   sequence.c_str(), script, append);
        if (mask == 0) {
            return TCL_ERROR;  // Tk set the error message
        }
        if (mask & ~kAllowedEventMask) {
            // Tk's canvas only allows key, button, motion, enter, leave and
            // virtual events for a tag binding.
            Tk_DeleteBinding(interp, widget.bindingTable, object, sequence.c_str());
            Tcl_SetObjResult(interp, Tcl_NewStringObj(
                "requested illegal events; only key, button, motion,"
                " enter, leave, and virtual events may be used", -1));
            Tcl_SetErrorCode(interp, "TK", "CANVAS", "BAD_EVENTS", nullptr);
            return TCL_ERROR;
        }
        widget.RememberBinding(object, sequence);
        Tcl_SetObjResult(interp, Tcl_NewStringObj("", -1));
        return TCL_OK;
    }

    if (objc == 4) {
        const std::string sequence = Tcl_GetString(objv[3]);
        const std::string virtualScript = widget.VirtualBinding(object, sequence);
        if (!virtualScript.empty()) {
            Tcl_SetObjResult(interp, Tcl_NewStringObj(virtualScript.c_str(), -1));
            return TCL_OK;
        }

        const char* command = Tk_GetBinding(interp, widget.bindingTable, object, sequence.c_str());
        if (!command) {
            // A missing binding is reported as an empty string; a malformed
            // event pattern is a real error (Tk left a message behind).
            const char* message = Tcl_GetString(Tcl_GetObjResult(interp));
            if (message && *message) return TCL_ERROR;
            Tcl_ResetResult(interp);
            Tcl_SetObjResult(interp, Tcl_NewStringObj("", -1));
            return TCL_OK;
        }
        Tcl_SetObjResult(interp, Tcl_NewStringObj(command, -1));
        return TCL_OK;
    }

    // ".c bind tagOrId" lists the sequences bound to that tag.
    Tcl_Obj* list = Tcl_NewObj();
    // Tk_GetAllBindings is not part of the public API; the sequences are
    // tracked by the widget instead.
    for (const std::string& sequence : widget.BoundSequences(object)) {
        Tcl_ListObjAppendElement(nullptr, list, Tcl_NewStringObj(sequence.c_str(), -1));
    }
    Tcl_SetObjResult(interp, list);
    return TCL_OK;
}

void CanvasDispatchEvent(CanvasWidget& widget, XEvent* eventPtr) {
    if (!widget.bindingTable || !eventPtr) return;

    const bool keyEvent = (eventPtr->type == KeyPress || eventPtr->type == KeyRelease);

    // Tk only dispatches to the tags of the item under the pointer; without a
    // current item no binding fires at all (Tk canvas semantics). Key events
    // are the exception: they fall back to the canvas wide "all" binding.
    const int itemId = widget.currentItemId;
    if (itemId == CanvasModel::kInvalidId && !keyEvent) return;

    const CanvasItem* item = (itemId == CanvasModel::kInvalidId) ? nullptr : widget.model.FindItem(itemId);

    // The order matters: the canvas itself ("all") first, then the tags of
    // the item in the order they were added, then the item itself (this is
    // the order Tk's canvas uses).
    std::vector<ClientData> objects;
    objects.reserve(item ? item->Tags().size() + 2 : 1);
    objects.push_back(AllBindingObject());
    if (item) {
        for (const std::string& tag : item->Tags()) {
            objects.push_back(reinterpret_cast<ClientData>(const_cast<char*>(Tk_GetUid(tag.c_str()))));
        }
        objects.push_back(ItemBindingObject(widget, itemId));
    }

    Tk_BindEvent(widget.bindingTable, eventPtr, widget.tkwin, static_cast<int>(objects.size()), objects.data());
}

}  // namespace tkd2dcanvas
