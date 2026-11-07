#include "CanvasEventBridge.h"

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

}  // namespace

void InstallEventHandlers(CanvasWidget& widget) {
    if (!widget.tkwin) return;

    // The mask uses X11 event masks (like Tk's own canvas does):
    // ExposureMask covers Expose, StructureNotifyMask covers
    // ConfigureNotify/MapNotify/UnmapNotify/DestroyNotify.
    Tk_CreateEventHandler(widget.tkwin,
                          ExposureMask | StructureNotifyMask | FocusChangeMask,
                          CanvasEventProc, &widget);
}

}  // namespace tkd2dcanvas
