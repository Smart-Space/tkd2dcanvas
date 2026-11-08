#include "CanvasScrollBridge.h"

#include <string>

#include "tkd2dcanvas/CanvasView.h"

#include "CanvasWidget.h"

namespace tkd2dcanvas {

namespace {

// Tk's ScrollFractions result: a list of two doubles.
Tcl_Obj* ViewFractionsObj(const CanvasView::Fraction& f) {
    Tcl_Obj* elements[2] = {Tcl_NewDoubleObj(f.first), Tcl_NewDoubleObj(f.last)};
    return Tcl_NewListObj(2, elements);
}

// The fraction string passed to the scroll command and returned by the
// xview/yview query (e.g. "0.0 0.2").
std::string FractionsString(const CanvasView::Fraction& f) {
    Tcl_Obj* list = ViewFractionsObj(f);
    std::string out = Tcl_GetString(list);
    Tcl_DecrRefCount(list);
    return out;
}

void ScrollbarsIdleProc(ClientData clientData) {
    auto* widget = static_cast<CanvasWidget*>(clientData);
    widget->scrollbarsPending = false;
    widget->BeginEvent();
    NotifyScrollCommand(*widget, true);
    NotifyScrollCommand(*widget, false);
    widget->EndEvent();  // frees the widget when it was destroyed meanwhile
}

}  // namespace

int HandleScrollCommand(CanvasWidget& widget, bool horizontal,
                        int objc, Tcl_Obj* const objv[]) {
    Tcl_Interp* interp = widget.interp;
    CanvasView& view = widget.model.View();

    if (objc == 2) {
        // Query form: report the visible span as {first last}.
        const CanvasView::Fraction f = horizontal ? view.XViewFraction()
                                                  : view.YViewFraction();
        Tcl_SetObjResult(interp, ViewFractionsObj(f));
        return TCL_OK;
    }

    double fraction = 0.0;
    int count = 0;
    const int type = Tk_GetScrollInfoObj(interp, objc, objv, &fraction, &count);

    const double oldX = view.XOffset();
    const double oldY = view.YOffset();

    switch (type) {
        case TK_SCROLL_MOVETO:
            view.MoveTo(horizontal, fraction);
            break;
        case TK_SCROLL_PAGES:
            view.ScrollPages(horizontal, count);
            break;
        case TK_SCROLL_UNITS:
            view.ScrollUnits(horizontal, count);
            break;
        default:
            return TCL_ERROR;
    }

    // The view changed: shift the retained pixels and redraw the strips plus
    // (deferred) scrollbar notification.
    widget.RepaintAfterScroll(oldX, oldY);
    RequestScrollbarUpdate(widget);
    Tcl_SetObjResult(interp, Tcl_NewStringObj("", -1));
    return TCL_OK;
}

void NotifyScrollCommand(CanvasWidget& widget, bool horizontal) {
    // Copy everything needed before evaluating the user script: it may
    // destroy the widget (Tk preserves the interp, the command strings and
    // the scroll geometry for the same reason).
    Tcl_Interp* interp = widget.interp;
    if (!interp) return;

    Tcl_Obj* commandObj = horizontal ? widget.xScrollCommandObj
                                     : widget.yScrollCommandObj;
    const std::string command = commandObj ? Tcl_GetString(commandObj) : "";
    if (command.empty()) return;

    const std::string fractions = horizontal
        ? FractionsString(widget.model.View().XViewFraction())
        : FractionsString(widget.model.View().YViewFraction());

    Tcl_Preserve(interp);

    Tcl_DString buf;
    Tcl_DStringInit(&buf);
    Tcl_DStringAppend(&buf, command.c_str(), -1);
    Tcl_DStringAppend(&buf, " ", -1);
    Tcl_DStringAppend(&buf, fractions.c_str(), -1);

    const int result = Tcl_EvalEx(interp, Tcl_DStringValue(&buf), -1, TCL_EVAL_GLOBAL);
    Tcl_DStringFree(&buf);

    if (result != TCL_OK) {
        Tcl_BackgroundException(interp, result);  // reports via bgerror
    }
    Tcl_ResetResult(interp);
    Tcl_Release(interp);
}

// Handles `.c scan mark x y` / `.c scan dragto x y ?gain?`. Direct
// translation of Tk's CANV_SCAN (tkCanvas.c): the mark records the pointer
// position and the current origin; dragto moves the view opposite to the
// pointer by gain times the distance (gain defaults to 10).
int HandleScanCommand(CanvasWidget& widget, int objc, Tcl_Obj* const objv[]) {
    Tcl_Interp* interp = widget.interp;

    static const char* const kOptions[] = {"mark", "dragto", nullptr};
    int index = -1;
    if (objc < 5) {
        Tcl_WrongNumArgs(interp, 2, objv, "mark|dragto x y ?dragGain?");
        return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[2], kOptions, "scan option", 0, &index) != TCL_OK) {
        return TCL_ERROR;
    }
    if ((objc != 5) && (objc != 5 + index)) {
        Tcl_WrongNumArgs(interp, 3, objv, index ? "x y ?gain?" : "x y");
        return TCL_ERROR;
    }
    int x = 0, y = 0;
    if (Tcl_GetIntFromObj(interp, objv[3], &x) != TCL_OK ||
        Tcl_GetIntFromObj(interp, objv[4], &y) != TCL_OK) {
        return TCL_ERROR;
    }
    int gain = 10;
    if (objc == 6 && Tcl_GetIntFromObj(interp, objv[5], &gain) != TCL_OK) {
        return TCL_ERROR;
    }

    if (!index) {
        widget.scanMarkX = x;
        widget.scanMarkY = y;
        widget.scanOriginX = widget.model.View().XOffset();
        widget.scanOriginY = widget.model.View().YOffset();
        Tcl_SetObjResult(interp, Tcl_NewStringObj("", -1));
        return TCL_OK;
    }

    const double newX = widget.scanOriginX - gain * (x - widget.scanMarkX);
    const double newY = widget.scanOriginY - gain * (y - widget.scanMarkY);
    const double oldX = widget.model.View().XOffset();
    const double oldY = widget.model.View().YOffset();
    widget.model.View().SetOrigin(newX, newY);
    widget.RepaintAfterScroll(oldX, oldY);
    RequestScrollbarUpdate(widget);
    Tcl_SetObjResult(interp, Tcl_NewStringObj("", -1));
    return TCL_OK;
}

void RequestScrollbarUpdate(CanvasWidget& widget) {
    if (widget.scrollbarsPending) return;  // coalesce
    widget.scrollbarsPending = true;
    Tcl_DoWhenIdle(&ScrollbarsIdleProc, &widget);
}

void CancelScrollbarUpdate(CanvasWidget& widget) {
    if (!widget.scrollbarsPending) return;
    Tcl_CancelIdleCall(&ScrollbarsIdleProc, &widget);
    widget.scrollbarsPending = false;
}

}  // namespace tkd2dcanvas
