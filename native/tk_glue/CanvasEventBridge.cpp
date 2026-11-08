#include "CanvasEventBridge.h"

#include "CanvasBindCommand.h"
#include "CanvasWidget.h"
#include "CanvasPointerState.h"

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
            for (auto& [id, binding] : widget->windowItemBindings) {
                binding.mapped = false;
                binding.lastRectValid = false;
            }
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
            // On a button press, first repick the current item with the
            // button state before the press (the pressed button does not
            // count yet, whether or not the event's state includes it),
            // then process the event.
            widget->buttonState = eventPtr->xbutton.state & ~ButtonMaskOf(eventPtr->xbutton.button);
            widget->PickCurrentItem(eventPtr->xbutton.x, eventPtr->xbutton.y, eventPtr);
            widget->buttonState = eventPtr->xbutton.state;
            CanvasDispatchEvent(*widget, eventPtr);
            break;

        case ButtonRelease:
            // On a button release, first process the event (with the button
            // state as reported), then repick with the released button
            // counted as up (whether or not the event's state includes it).
            widget->buttonState = eventPtr->xbutton.state;
            CanvasDispatchEvent(*widget, eventPtr);
            widget->buttonState = eventPtr->xbutton.state & ~ButtonMaskOf(eventPtr->xbutton.button);
            widget->PickCurrentItem(eventPtr->xbutton.x, eventPtr->xbutton.y, eventPtr);
            break;

        case MotionNotify:
            widget->buttonState = eventPtr->xmotion.state;
            widget->PickCurrentItem(eventPtr->xmotion.x, eventPtr->xmotion.y, eventPtr);
            CanvasDispatchEvent(*widget, eventPtr);
            break;

        case EnterNotify:
        case LeaveNotify:
            // Pointer entered/left the widget. Tk dispatches no item binding
            // for the raw event; the repick synthesizes enter/leave events
            // for the items involved.
            widget->buttonState = eventPtr->xcrossing.state;
            widget->PickCurrentItem(eventPtr->xcrossing.x, eventPtr->xcrossing.y, eventPtr);
            break;

        default:
            CanvasDispatchEvent(*widget, eventPtr);
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
