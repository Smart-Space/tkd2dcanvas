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
#include <unordered_map>
#include <vector>

#include "tkd2dcanvas/CanvasItem.h"
#include "tkd2dcanvas/CanvasModel.h"
#include "tkd2dcanvas/Color.h"

#include "Win32Surface.h"
#include "CanvasTkWindowItem.h"

namespace tkd2dcanvas {

// The tag Tk puts on the item under the pointer.
inline constexpr const char* kCurrentTag = "current";

struct CanvasWidget {
    // ---- option values (targets of the Tk option table) --------------------
    Tcl_Obj* backgroundObj = nullptr;      // -background (colour name)
    Tcl_Obj* cursorObj = nullptr;          // -cursor
    Tcl_Obj* takeFocusObj = nullptr;       // -takefocus
    Tcl_Obj* classObj = nullptr;           // -class
    Tcl_Obj* scrollRegionObj = nullptr;    // -scrollregion
    Tcl_Obj* xScrollCommandObj = nullptr;  // -xscrollcommand
    Tcl_Obj* yScrollCommandObj = nullptr;  // -yscrollcommand
    Tcl_Obj* highlightBackgroundObj = nullptr;  // -highlightbackground
    Tcl_Obj* highlightColorObj = nullptr;       // -highlightcolor
    Tcl_Obj* insertBackgroundObj = nullptr;     // -insertbackground
    Tcl_Obj* selectBackgroundObj = nullptr;     // -selectbackground
    Tcl_Obj* selectForegroundObj = nullptr;     // -selectforeground
    Tcl_Obj* rendererObj = nullptr;              // -renderer (d2d|gdi)
    int width = 400;                       // -width
    int height = 300;                      // -height
    int borderWidth = 0;                   // -borderwidth
    int highlightWidth = 0;                // -highlightthickness
    int selectBorderWidth = 0;             // -selectborderwidth
    int insertBorderWidth = 0;             // -insertborderwidth
    int insertWidth = 2;                   // -insertwidth
    int relief = 0;                        // -relief
    int xScrollIncrement = 0;              // -xscrollincrement
    int yScrollIncrement = 0;              // -yscrollincrement
    double closeEnough = 1.0;              // -closeenough
    int confine = 1;                       // -confine
    int aaFactor = 4;                      // -antialiasfactor

    // ---- runtime state -----------------------------------------------------
    Tcl_Interp* interp = nullptr;
    Tk_Window tkwin = nullptr;
    Tk_OptionTable optionTable = nullptr;
    Tcl_Command command = nullptr;
    std::string path;

    unsigned buttonState = 0;
    bool leftGrabbedItem = false;
    int repickDepth = 0;

    CanvasModel model;
    Win32Surface surface;
    Color backgroundColor{255, 255, 255};

    HWND hwnd = nullptr; // the widget's own Win32 window ("TkChild")
    int pixelWidth = 0;
    int pixelHeight = 0;
    bool mapped = false;

    Tk_BindingTable bindingTable = nullptr;
    int currentItemId = CanvasModel::kInvalidId;
    // Tk: the item that receives keyboard events (".c focus <tagOrId>").
    // Only items supporting the insertion cursor may hold the focus; it is
    // cleared when the item is deleted.
    int focusItemId = CanvasModel::kInvalidId;
    // Per item binding objects
    std::unordered_map<int, ClientData> itemBindingObjects;
    // The event patterns bound to a binding object.
    std::unordered_map<void*, std::vector<std::string>> boundSequences;
    // Virtual event patterns ("<<Foo>>"): Tk's canvas stores them but never
    // fires them, so they are kept aside instead of going into Tk's table.
    std::unordered_map<void*, std::unordered_map<std::string, std::string>> virtualBindings;

    int eventDepth = 0;
    bool destroyPending = false;
    bool scrollbarsPending = false;
    // True while a repositioning of the embedded windows is queued.
    bool windowItemSyncPending = false;
    WindowItemBindings windowItemBindings;
    int scanMarkX = 0;
    int scanMarkY = 0;
    double scanOriginX = 0.0;
    double scanOriginY = 0.0;

    // Creates the widget (window + options + command + event handlers).
    // `objv[firstOption..]` are the option words of the creating command.
    // Returns nullptr with an interpreter error set on failure.
    static CanvasWidget* Create(Tcl_Interp* interp, const char* pathName,
                                int objc, Tcl_Obj* const objv[], int firstOption);

    static int WidgetCmd(ClientData clientData, Tcl_Interp* interp,
                         int objc, Tcl_Obj* const objv[]);
    static void CommandDeleted(ClientData clientData);

    // Re-reads the options and applies them (geometry, colours, scroll area).
    // Returns false when an option value was rejected.
    bool ApplyOptions(int mask);

    // Refreshes the cached geometry from Tk and resizes the drawing surface.
    void UpdateGeometry();

    // Marks the widget area as damaged (repaint on the next WM_PAINT).
    void Invalidate();
    // Same, limited to a canvas-unit rectangle
    void Invalidate(const BBox& box);

    // Queues a repositioning of the embedded windows (the canvas' "window"
    // items) from an idle callback.  The windows are then moved once per
    // event cycle, with the final position - like Tk's canvas, which
    // positions its window items while redrawing.
    void RequestWindowItemSync();

    // Cancels a queued repositioning of the embedded windows (the widget
    // must not be touched by a pending idle callback after it is gone).
    void CancelWindowItemSync();

    // Draws the widget with Direct2D into `dc` (a DC of the widget window).
    // `damaged` are the rectangles that have to be repainted (window
    // coordinates); only the parts that touch the widget are painted.
    void Paint(HDC dc, const std::vector<RECT>& damaged);

    // Called after the view origin changed from (oldX, oldY) to its current
    // value. When the shift is an integral number of pixels that fits inside
    // the window, the window pixels and the retained back buffer are scrolled
    // and only the newly exposed strips (plus any damage that was pending
    // before the change) are invalidated. Everything else falls back to a
    // full repaint. The window is repainted synchronously before the call
    // returns, so no stale (unpainted) strips are ever visible.
    void RepaintAfterScroll(double oldX, double oldY);

    // Picks the item under the given (widget local) coordinates and makes it
    // the "current" item. The hit test itself lives in the C++ core. As a
    // side effect, synthesized <Enter>/<Leave> events are dispatched to the
    // tag bindings when the current item changes (Tk: PickCurrentItem).
    // `event` is the pointer event that triggered the pick; its copy is used
    // as the template for the synthesized events. May be nullptr, in which
    // case no synthesized events are dispatched.
    void PickCurrentItem(int x, int y, XEvent* event);
    // Clears the current item (the pointer left the widget or the item was
    // deleted). Does not synthesize events.
    void ClearCurrentItem();

    // The event patterns bound to a binding object (empty when there is none).
    std::vector<std::string> BoundSequences(ClientData object) const;
    // Remembers that `sequence` is bound to `object`.
    void RememberBinding(ClientData object, const std::string& sequence);
    // Forgets a binding that was just removed.
    void ForgetBinding(ClientData object, const std::string& sequence);
    // Stores a virtual event pattern (never fired, like in Tk).
    void RememberVirtualBinding(ClientData object, const std::string& sequence,
                                 const std::string& script);
    // The script of a virtual event pattern (empty when there is none).
    std::string VirtualBinding(ClientData object, const std::string& sequence) const;
    // Removes a virtual event pattern.
    void ForgetVirtualBinding(ClientData object, const std::string& sequence);

    // Marks the beginning of an event dispatch. The widget must not be freed
    // while an event is being dispatched.
    void BeginEvent();
    // Tears a half-built widget down like a normal destroy.
    static void AbortCreate(Tk_Window tkwin, CanvasWidget& raw);
    // Marks the end of an event dispatch; frees the widget when it was
    // destroyed in the meantime. Nothing may touch the widget afterwards.
    void EndEvent();
    // Requests the destruction of the widget.
    void RequestDestroy();

private:
    // Detaches the widget from Tk and frees it.
    void FinalizeDestroy();
};

}  // namespace tkd2dcanvas
