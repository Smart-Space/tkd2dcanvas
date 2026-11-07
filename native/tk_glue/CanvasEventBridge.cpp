#include "CanvasEventBridge.h"

#include "CanvasWidget.h"

namespace tkd2dcanvas {

void InstallEventHandlers(CanvasWidget& widget) {
    // TODO(M6): Tk_CreateEventHandler for ButtonPress/ButtonRelease/Motion/
    // Enter/Leave/KeyPress/KeyRelease/FocusIn/FocusOut/Configure/Expose/Destroy.
    // Each handler maps the Tk event onto CanvasInputEvent, performs hit-test
    // via CanvasModel, then either fires a stored bind script (Tcl_EvalObjv)
    // or forwards to the default scroll/focus logic in CanvasScrollBridge.
    (void)widget;
}

void RemoveEventHandlers(CanvasWidget& widget) {
    // TODO(M6): Tk_DeleteEventHandler for every eventMask installed above.
    (void)widget;
}

}  // namespace tkd2dcanvas
