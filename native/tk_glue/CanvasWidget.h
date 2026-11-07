// CanvasWidget.h — the Tk widget record for the D2D canvas.
//
// The widget is a *real* Tk child window (so Tk keeps doing geometry
// management, event routing, focus and bindings) whose content is painted
// with Direct2D into the top-level window's DC (see ToplevelPainter.h).
#pragma once

#include <tcl.h>
#include <tk.h>
#include <windows.h>

#include <string>

#include "tkd2dcanvas/CanvasModel.h"
#include "tkd2dcanvas/Color.h"

#include "Win32Surface.h"

namespace tkd2dcanvas {

struct CanvasWidget {
    // ---- option values (targets of the Tk option table) --------------------
    Tcl_Obj* backgroundObj = nullptr;      // -background (colour name)
    Tcl_Obj* cursorObj = nullptr;          // -cursor
    Tcl_Obj* takeFocusObj = nullptr;       // -takefocus
    Tcl_Obj* classObj = nullptr;           // -class
    Tcl_Obj* scrollRegionObj = nullptr;    // -scrollregion
    Tcl_Obj* xScrollCommandObj = nullptr;  // -xscrollcommand
    Tcl_Obj* yScrollCommandObj = nullptr;  // -yscrollcommand
    int width = 400;                       // -width
    int height = 300;                      // -height
    int borderWidth = 0;                   // -borderwidth
    int highlightWidth = 0;                // -highlightthickness
    int relief = 0;                        // -relief
    int xScrollIncrement = 0;              // -xscrollincrement
    int yScrollIncrement = 0;              // -yscrollincrement
    double closeEnough = 2.0;              // -closeenough
    int confine = 1;                       // -confine

    // ---- runtime state -----------------------------------------------------
    Tcl_Interp* interp = nullptr;
    Tk_Window tkwin = nullptr;
    Tk_OptionTable optionTable = nullptr;
    Tcl_Command command = nullptr;
    std::string path;

    CanvasModel model;
    Win32Surface surface;
    Color backgroundColor{255, 255, 255};

    HWND hwnd = nullptr; // the widget's own Win32 window ("TkChild")
    int pixelWidth = 0;
    int pixelHeight = 0;
    bool mapped = false;

    // Creates the widget (window + options + command + event handlers).
    // `objv[firstOption..]` are the option words of the creating command.
    // Returns nullptr with an interpreter error set on failure.
    static CanvasWidget* Create(Tcl_Interp* interp, const char* pathName,
                                int objc, Tcl_Obj* const objv[], int firstOption);

    static int WidgetCmd(ClientData clientData, Tcl_Interp* interp,
                         int objc, Tcl_Obj* const objv[]);
    static void CommandDeleted(ClientData clientData);

    // Re-reads the options and applies them (geometry, colours, scroll area).
    void ApplyOptions(int mask);

    // Refreshes the cached geometry from Tk and resizes the drawing surface.
    void UpdateGeometry();

    // Marks the widget area as damaged (repaint on the next WM_PAINT).
    void Invalidate();

    // Draws the widget with Direct2D into `dc` (a DC of the top-level window).
    // `damaged` is the area that has to be repainted (in top-level client
    // coordinates); the widget is only painted when the two overlap.
    void Paint(HDC dc, const RECT& damaged);
};

}  // namespace tkd2dcanvas
