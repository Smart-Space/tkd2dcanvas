// CanvasWidget.h — the Tk widget record. Owns the CanvasModel, the surface
// and the registered Tcl interpreter commands for the widget path.
#pragma once

#include <tcl.h>
#include <tk.h>

#include <memory>
#include <string>

#include "tkd2dcanvas/CanvasModel.h"
#include "tkd2dcanvas/Color.h"

#include "Win32Surface.h"
#include "CanvasOptions.h"

namespace tkd2dcanvas {

struct CanvasWidget {
    Tcl_Interp* interp = nullptr;
    Tk_Window tkwin = nullptr;
    std::string path;

    CanvasModel model;
    Win32Surface surface;
    CanvasOptions options;
    Color background{255, 255, 255};

    // Creates the Tk window (M2) and registers the per-widget command (M1).
    // Returns nullptr and sets interp error on failure.
    static CanvasWidget* Create(Tcl_Interp* interp, const char* pathName, const CanvasOptions& options);
    static void Destroy(void* clientData);

    // Redraw helper used by Configure / item commands / scroll changes.
    void InvalidateAll() { surface.InvalidateAll(); }
};

}  // namespace tkd2dcanvas
