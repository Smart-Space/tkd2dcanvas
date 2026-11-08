// CanvasTkWindowItem.h — glue for canvas "window" items (M10).
//
// The C++ item core (native/items/WindowItem.*) is Tk-free; everything that
// touches the embedded Tk window lives here. The structure and the order of
// operations follow Tk's canvas window item (tkCanvWind.c) as closely as the
// codebase allows:
//
//   create/itemconfigure -window  -> validate (Tk_NameToWindow + hierarchy
//                                    walk), then Tk_ManageGeometry() so the
//                                    canvas acts as the geometry manager of
//                                    the embedded window;
//   embedded window requests size -> the geometry manager's requestProc gets
//                                    the new natural size, the item bbox is
//                                    recomputed and the child is repositioned;
//   embedded window destroyed     -> a StructureNotify handler clears the
//                                    item's -window (1x1 bbox); the binding is
//                                    dropped;
//   view origin / item geometry /
//   canvas size changes          -> SyncWindowItems() repositions and
//                                    maps/unmaps the child, like Tk's
//                                    DisplayWinItem() during redraws;
//   delete / canvas teardown     -> Tk_ManageGeometry(NULL) + unmaintain +
//                                    unmap (Tk's DeleteWinItem()).
#pragma once

#include <tcl.h>
#include <tk.h>

#include <limits>
#include <string>
#include <unordered_map>

namespace tkd2dcanvas {

struct CanvasWidget;

struct WindowItemBinding {
    Tk_Window tkwin = nullptr;  // non-owning
    CanvasWidget* widget = nullptr;
    int itemId = -1;
    int lastX = 0;
    int lastY = 0;
    int lastW = 0;
    int lastH = 0;
    bool lastRectValid = false;
    bool mapped = false;  // we (the canvas) currently keep it mapped/maintained
};

using WindowItemBindings = std::unordered_map<int, WindowItemBinding>;

// StructureNotify handler for the embedded window (only DestroyNotify is of
// interest: it clears the item's -window like Tk's WinItemStructureProc).
// Registered per bound window; the clientData is the &binding element.
void CanvasTkWindowStructureProc(ClientData clientData, XEvent* eventPtr);

// Resolves and validates `path` for use as the -window of a window item in
// `widget`, following Tk's ConfigureWinItem checks (existence, hierarchy,
// top-level/canvas rejection). Returns false with the interpreter error set.
bool ValidateWindowItemPath(Tcl_Interp* interp, CanvasWidget& widget, const std::string& path, Tk_Window& outWindow);

// Creates/updates the binding for the item to point at `path` ("" detaches).
// Manages the new window with the canvas geometry manager and unmanages the
// previous one, like Tk's ConfigureWinItem. Cannot fail once the path has
// been validated (it is validated again here; a failure leaves the item
// without a window).
void AttachWindowItem(CanvasWidget& widget, int itemId);

// Unmaps the embedded window and unmanages it (Tk's DeleteWinItem).
// Idempotent; safe to call for items without a window item binding.
void DetachWindowItem(CanvasWidget& widget, int itemId);

// Positions every bound embedded window according to the item bbox and the
// current view origin, and maps/unmaps it following Tk's DisplayWinItem
// rules (hidden item or fully-offscreen child -> unmap/unmaintain, else map
// and move). Also garbage-collects dead bindings (destroyed window).
// Cheap and safe to call often: only changed rectangles are applied.
void SyncWindowItems(CanvasWidget& widget);

// Widget teardown: unmanages and unmaps every embedded window. Called from
// FinalizeDestroy; afterwards no binding keeps a Tk reference alive.
void ShutdownWindowItems(CanvasWidget& widget);

}  // namespace tkd2dcanvas
