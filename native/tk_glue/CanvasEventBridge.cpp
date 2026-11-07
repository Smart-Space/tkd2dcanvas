#include "CanvasEventBridge.h"

#include "CanvasBindCommand.h"
#include "CanvasWidget.h"

namespace tkd2dcanvas {

namespace {

void CanvasEventProc(ClientData clientData, XEvent* eventPtr) {
    auto* widget = static_cast<CanvasWidget*>(clientData);
    if (!widget || !eventPtr) return;

    switch (eventPtr->type) {
        case ConfigureNotify:
            widget->UpdateGeometry();
            widget->Invalidate();
            break;

        case Expose:
            // The window hook paints the canvas right after Tk painted the
            // widget's background, so there is nothing to invalidate here
            // (invalidating would cause an endless paint loop).
            widget->UpdateGeometry();
            break;

        case MapNotify:
            widget->mapped = true;
            widget->UpdateGeometry();
            widget->Invalidate();
            break;

        case UnmapNotify:
            widget->mapped = false;
            break;

        case DestroyNotify:
            // Deleting our Tcl command triggers CanvasWidget::CommandDeleted,
            // which frees the widget. Do not touch `widget` afterwards.
            if (widget->command) {
                Tcl_DeleteCommandFromToken(widget->interp, widget->command);
            }
            break;

        default:
            break;
    }
}

// Handles the pointer and keyboard events of the canvas.
//
// This mirrors Tk's own canvas: the current item is picked with the C++ core
// hit test and the event is then dispatched to the tag bindings.
void CanvasInputEventProc(ClientData clientData, XEvent* eventPtr) {
    auto* widget = static_cast<CanvasWidget*>(clientData);
    if (!widget || !eventPtr) return;

    // A binding script may destroy the canvas: the widget is therefore kept
    // alive until the dispatch is over.
    widget->BeginEvent();

    switch (eventPtr->type) {
        case ButtonPress:
        case MotionNotify:
        case EnterNotify:
            widget->PickCurrentItem(eventPtr->xbutton.x, eventPtr->xbutton.y);
            break;
        default:
            break;
    }

    CanvasDispatchEvent(*widget, eventPtr);

    switch (eventPtr->type) {
        case ButtonRelease:
            // The pointer may have moved away while the button was down: pick
            // the current item again (Tk does the same).
            widget->PickCurrentItem(eventPtr->xbutton.x, eventPtr->xbutton.y);
            break;
        case LeaveNotify:
            // The pointer left the widget: there is no current item any more.
            widget->ClearCurrentItem();
            break;
        default:
            break;
    }

    // Must be the last statement: the widget may be freed here.
    widget->EndEvent();
}

}  // namespace

void InstallEventHandlers(CanvasWidget& widget) {
    if (!widget.tkwin) return;

    // The mask uses X11 event masks (like Tk's own canvas does):
    // ExposureMask covers Expose, StructureNotifyMask covers
    // ConfigureNotify/MapNotify/UnmapNotify/DestroyNotify.
    Tk_CreateEventHandler(widget.tkwin,
                          ExposureMask | StructureNotifyMask | FocusChangeMask,
                          CanvasEventProc, &widget);

    // The pointer and keyboard events are needed by the tag bindings.
    Tk_CreateEventHandler(widget.tkwin,
                          KeyPressMask | KeyReleaseMask | ButtonPressMask |
                            ButtonReleaseMask | EnterWindowMask |
                            LeaveWindowMask | PointerMotionMask,
                          CanvasInputEventProc, &widget);
}

}  // namespace tkd2dcanvas
