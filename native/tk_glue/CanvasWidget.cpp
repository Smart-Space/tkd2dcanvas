#include "CanvasWidget.h"

#include "CanvasTkColor.h"
#include "CanvasItemCommand.h"

namespace tkd2dcanvas {

CanvasWidget* CanvasWidget::Create(Tcl_Interp* interp, const char* pathName, const CanvasOptions& options)
{
    // Validate the path with Tk (M2 will create the real window).
    Tk_Window parent = Tk_MainWindow(interp);
    if (!parent) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("cannot get Tk main window", -1));
        return nullptr;
    }

    auto widget = std::make_unique<CanvasWidget>();
    widget->interp = interp;
    widget->options = options;

    if (!ResolveTkColor(interp, parent, options.background.c_str(), widget->background)) {
        return nullptr;  // ResolveTkColor sets interp error
    }

    // TODO(M2): Tk_CreateWindow + Tk_SetClass("Canvas") + Tk_MakeWindowExist,
    // geometry request, option tables via Tk_CreateOptionTable,
    // Tcl_CreateObjCommand for the widget path, Tk_CreateEventHandler for
    // Expose/Configure/ButtonPress..., Tk_GeometryRequest.
    // For now: register the widget command only, fail creation honestly.
    (void)pathName;

    Tcl_SetObjResult(interp, Tcl_NewStringObj(
        "tkd2dcanvas: widget creation is not implemented yet (milestones M1/M2)", -1));
    return nullptr;
}

void CanvasWidget::Destroy(void* clientData)
{
    auto* widget = static_cast<CanvasWidget*>(clientData);
    if (!widget) return;
    // TODO(M2): remove the widget command, destroy Tk window, delete widget.
    delete widget;
}

}  // namespace tkd2dcanvas
