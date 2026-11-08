// CanvasTkWindowItem.cpp — glue for canvas "window" items (M10).
//
// Mirrors Tk's canvas window item (tkCanvWind.c / cWindow.c): an embedded
// Tk window is managed by this canvas acting as its geometry manager
// (Tk_ManageGeometry), which is what makes the natural size changes of the
// embedded widget flow back into the item bbox. Placement happens in
// SyncWindowItems, the equivalent of Tk's DisplayWinItem called during
// redraws — except we call it eagerly from the scroll/resize/item-change
// paths instead of a redraw cycle.
#include "CanvasTkWindowItem.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "tkd2dcanvas/CanvasModel.h"
#include "tkd2dcanvas/BBox.h"

#include "WindowItem.h"
#include "CanvasWidget.h"

namespace tkd2dcanvas {

namespace {

double RoundHalfAway(double v) {
    return v >= 0.0 ? std::floor(v + 0.5) : std::ceil(v - 0.5);
}

WindowItem* FindWindowItem(CanvasWidget& widget, int itemId) {
    CanvasItem* raw = widget.model.FindItem(itemId);
    if (raw && raw->Type() == "window") return static_cast<WindowItem*>(raw);
    return nullptr;
}

// Invalidates a widget-local rectangle on the canvas window (goes through
// the normal WM_PAINT path, which repaints it from the D2D back buffer).
void InvalidateWidgetRect(CanvasWidget& widget, int x, int y, int w, int h) {
    if (w <= 0 || h <= 0 || !widget.hwnd) return;
    RECT rc{};
    rc.left = static_cast<LONG>(x);
    rc.top = static_cast<LONG>(y);
    rc.right = static_cast<LONG>(x + w);
    rc.bottom = static_cast<LONG>(y + h);
    // The canvas has no border inset; clip to the client area.
    const RECT bounds{0, 0, static_cast<LONG>(widget.pixelWidth),
                      static_cast<LONG>(widget.pixelHeight)};
    RECT clipped{};
    if (!IntersectRect(&clipped, &rc, &bounds)) return;
    InvalidateRect(widget.hwnd, &clipped, FALSE);
}

void InvalidateWidgetRect(CanvasWidget& widget, const WindowItemBinding& b) {
    InvalidateWidgetRect(widget, b.lastX, b.lastY, b.lastW, b.lastH);
}

// Temporarily hides an embedded window: the child-of-canvas case is unmapped
// directly; the sibling case is unmaintained (Tk_UnmaintainGeometry removes the
// maintained relationship and unmaps, undoing Tk_ManageGeometry's effect on
// the slave's visibility). The binding stays registered: the next
// SyncWindowItems call for a visible item re-creates the maintain state.
void ReleaseBinding(CanvasWidget& widget, WindowItemBinding& b) {
    if (!b.tkwin) return;
    if (widget.tkwin && Tk_Parent(b.tkwin) == widget.tkwin) {
        Tk_UnmapWindow(b.tkwin);
    } else if (widget.tkwin) {
        Tk_UnmaintainGeometry(b.tkwin, widget.tkwin);
    }
    b.mapped = false;
    b.lastRectValid = false;
}

// Definitively detaches an embedded window: stop acting as its geometry
// manager, remove the structure handler, unmaintain (when not a child of the
// canvas) and unmap. Mirrors Tk's DeleteWinItem.
void UnmanageBinding(CanvasWidget& widget, WindowItemBinding& b) {
    if (!b.tkwin) return;
    Tk_DeleteEventHandler(b.tkwin, StructureNotifyMask, &CanvasTkWindowStructureProc, &b);
    Tk_ManageGeometry(b.tkwin, nullptr, nullptr);
    if (widget.tkwin && Tk_Parent(b.tkwin) != widget.tkwin) {
        Tk_UnmaintainGeometry(b.tkwin, widget.tkwin);
    }
    Tk_UnmapWindow(b.tkwin);
    b.tkwin = nullptr;
    b.mapped = false;
    b.lastRectValid = false;
}

void SetNaturalSizeFromTk(CanvasWidget& widget, int itemId, Tk_Window embedded) {
    if (WindowItem* wi = FindWindowItem(widget, itemId)) {
        wi->SetNaturalSize(Tk_ReqWidth(embedded), Tk_ReqHeight(embedded));
    }
}

void CanvasTkWindowRequestProc(ClientData clientData, Tk_Window /*tkwin*/);
void CanvasTkWindowLostContentProc(ClientData clientData, Tk_Window /*tkwin*/);

// Tk: the canvas acts as the geometry manager of every embedded window
// (registered through Tk_ManageGeometry), which is how a natural-size change
// of the embedded widget flows back into the item bbox.
const Tk_GeomMgr kCanvasGeomType = {
    "canvas",                    // name
    CanvasTkWindowRequestProc,   // requestProc
    CanvasTkWindowLostContentProc, // lostContentProc
};

void CanvasTkWindowRequestProc(ClientData clientData, Tk_Window /*tkwin*/) {
    auto* binding = static_cast<WindowItemBinding*>(clientData);
    if (!binding || !binding->widget) return;
    CanvasWidget& widget = *binding->widget;
    WindowItem* wi = FindWindowItem(widget, binding->itemId);
    if (!wi || binding->tkwin == nullptr) return;

    const BBox before = wi->GetBBox();
    SetNaturalSizeFromTk(widget, binding->itemId, binding->tkwin);
    const BBox after = wi->GetBBox();
    if (before != after) {
        // The item's model geometry changed size: repaint both sides. The
        // repositioning of the child happens in the sync below.
        widget.Invalidate(before);
        widget.Invalidate(after);
    }
    SyncWindowItems(widget);
}

void CanvasTkWindowLostContentProc(ClientData clientData, Tk_Window tkwin) {
    // Tk's WinItemLostContentProc: another geometry manager took over the
    // embedded window. Drop our structure handler, unmaintain and unmap it,
    // and forget the binding (the item reverts to a 1x1 bbox).
    auto* binding = static_cast<WindowItemBinding*>(clientData);
    if (!binding) return;
    CanvasWidget* widget = binding->widget;
    BBox box = BBox::Empty();
    bool found = false;
    if (widget && tkwin) {
        Tk_DeleteEventHandler(tkwin, StructureNotifyMask,
                              &CanvasTkWindowStructureProc, binding);
        if (widget->tkwin && Tk_Parent(tkwin) != widget->tkwin) {
            Tk_UnmaintainGeometry(tkwin, widget->tkwin);
        }
        Tk_UnmapWindow(tkwin);
        if (WindowItem* wi = FindWindowItem(*widget, binding->itemId)) {
            box = wi->GetBBox();
            found = true;
            wi->SetWindowPath("");
        }
    }
    binding->tkwin = nullptr;
    binding->mapped = false;
    binding->lastRectValid = false;
    if (widget && found) widget->Invalidate(box);
}

}  // namespace

void CanvasTkWindowStructureProc(ClientData clientData, XEvent* eventPtr) {
    auto* binding = static_cast<WindowItemBinding*>(clientData);
    if (!binding || !eventPtr) return;
    if (eventPtr->type != DestroyNotify) return;

    binding->tkwin = nullptr;
    binding->mapped = false;
    binding->lastRectValid = false;
    if (!binding->widget) return;
    CanvasWidget& widget = *binding->widget;
    if (WindowItem* wi = FindWindowItem(widget, binding->itemId)) {
        const BBox box = wi->GetBBox();
        wi->SetWindowPath("");
        widget.Invalidate(box);
    }
}

bool ValidateWindowItemPath(Tcl_Interp* interp, CanvasWidget& widget,
                            const std::string& path, Tk_Window& outWindow) {
    outWindow = nullptr;
    if (path.empty()) return true;

    Tk_Window w = Tk_NameToWindow(interp, path.c_str(), widget.tkwin);
    if (!w) {
        return false;  // Tk_NameToWindow set "bad window path name ..."
    }

    bool found = false;
    for (Tk_Window ancestor = widget.tkwin; ancestor != nullptr;
         ancestor = Tk_Parent(ancestor)) {
        if (ancestor == Tk_Parent(w)) {
            found = true;
            break;
        }
        if (Tk_TopWinHierarchy(ancestor)) break;
    }

    if (!found || w == widget.tkwin || Tk_TopWinHierarchy(w)) {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("can't use %s in a window item of this canvas", path.c_str()));
        Tcl_SetErrorCode(interp, "TK", "GEOMETRY", "HIERARCHY", nullptr);
        return false;
    }
    outWindow = w;
    return true;
}

void AttachWindowItem(CanvasWidget& widget, int itemId) {
    WindowItem* wi = FindWindowItem(widget, itemId);
    if (!wi || !widget.tkwin) return;

    const std::string& path = wi->WindowPath();
    if (path.empty()) {
        DetachWindowItem(widget, itemId);
        return;
    }

    Tk_Window embedded = Tk_NameToWindow(widget.interp, path.c_str(), widget.tkwin);
    if (!embedded) {
        DetachWindowItem(widget, itemId);
        return;
    }

    auto it = widget.windowItemBindings.find(itemId);
    if (it != widget.windowItemBindings.end() && it->second.tkwin == embedded) {
        SetNaturalSizeFromTk(widget, itemId, embedded);
        return;
    }

    DetachWindowItem(widget, itemId);

    WindowItemBinding binding;
    binding.tkwin = embedded;
    binding.widget = &widget;
    binding.itemId = itemId;
    auto [pos, inserted] = widget.windowItemBindings.emplace(itemId, std::move(binding));
    WindowItemBinding& b = pos->second;
    (void)inserted;

    Tk_CreateEventHandler(embedded, StructureNotifyMask, &CanvasTkWindowStructureProc, &b);
    Tk_ManageGeometry(embedded, &kCanvasGeomType, &b);

    SetNaturalSizeFromTk(widget, itemId, embedded);
}

void DetachWindowItem(CanvasWidget& widget, int itemId) {
    if (!widget.tkwin) {
        widget.windowItemBindings.erase(itemId);
        return;
    }
    auto it = widget.windowItemBindings.find(itemId);
    if (it == widget.windowItemBindings.end()) return;

    const BBox box = FindWindowItem(widget, itemId)
        ? FindWindowItem(widget, itemId)->GetBBox()
        : BBox::Empty();

    if (it->second.mapped && it->second.lastRectValid) {
        InvalidateWidgetRect(widget, it->second);
    }
    UnmanageBinding(widget, it->second);
    widget.windowItemBindings.erase(it);
    widget.Invalidate(box);
}

void SyncWindowItems(CanvasWidget& widget) {
    if (!widget.tkwin) return;

    const CanvasView& view = widget.model.View();
    const double vx = view.XOffset();
    const double vy = view.YOffset();
    const int viewW = widget.pixelWidth;
    const int viewH = widget.pixelHeight;

    for (auto it = widget.windowItemBindings.begin();
         it != widget.windowItemBindings.end();) {
        WindowItemBinding& b = it->second;
        WindowItem* wi = FindWindowItem(widget, b.itemId);

        if (!wi || b.tkwin == nullptr || wi->WindowPath().empty()) {
            if (b.tkwin) {
                if (b.mapped && b.lastRectValid) InvalidateWidgetRect(widget, b);
                UnmanageBinding(widget, b);
            }
            it = widget.windowItemBindings.erase(it);
            continue;
        }

        const BBox box = wi->GetBBox();  // Empty() when hidden
        const bool hidden = box.IsValid() == false || wi->IsHidden();

        if (hidden) {
            if (b.mapped) {
                if (b.lastRectValid) InvalidateWidgetRect(widget, b);
                ReleaseBinding(widget, b);
            }
            ++it;
            continue;
        }

        // Tk's Tk_CanvasWindowCoords: canvas int origin - view origin,
        // rounded half away from zero; the item bbox is integral already.
        const int x = static_cast<int>(RoundHalfAway(box.left - vx));
        const int y = static_cast<int>(RoundHalfAway(box.top - vy));
        const int w = std::max(1, static_cast<int>(box.Width()));
        const int h = std::max(1, static_cast<int>(box.Height()));

        // Tk unmaps the slave when it is completely outside the visible area
        // (this used to prevent surprise reappearances in real Tk too).
        const bool offscreen =
            (x + w <= 0) || (y + h <= 0) || (x >= viewW) || (y >= viewH);

        if (offscreen) {
            if (b.mapped) {
                if (b.lastRectValid) InvalidateWidgetRect(widget, b);
                ReleaseBinding(widget, b);
            }
            ++it;
            continue;
        }

        const bool isCanvasParent = Tk_Parent(b.tkwin) == widget.tkwin;
        const bool sameRect = b.mapped && b.lastRectValid &&
                              b.lastX == x && b.lastY == y &&
                              b.lastW == w && b.lastH == h;

        if (isCanvasParent) {
            if (sameRect) {
                ++it;
                continue;
            }
            if (b.mapped && b.lastRectValid) {
                InvalidateWidgetRect(widget, b);
            }
            Tk_MoveResizeWindow(b.tkwin, x, y, w, h);
            Tk_MapWindow(b.tkwin);
        } else {
            // The child is a sibling (child of an ancestor): Tk keeps it in
            // sync through MaintainGeometry so it follows the canvas around.
            if (sameRect) {
                ++it;
                continue;
            }
            if (b.mapped && b.lastRectValid) {
                InvalidateWidgetRect(widget, b);
            }
            Tk_MaintainGeometry(b.tkwin, widget.tkwin, x, y, w, h);
        }

        b.lastX = x;
        b.lastY = y;
        b.lastW = w;
        b.lastH = h;
        b.lastRectValid = true;
        b.mapped = true;
        ++it;
    }
}

void ShutdownWindowItems(CanvasWidget& widget) {
    for (auto& [id, b] : widget.windowItemBindings) {
        UnmanageBinding(widget, b);
    }
    widget.windowItemBindings.clear();
}

}  // namespace tkd2dcanvas
