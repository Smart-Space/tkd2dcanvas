#include "CanvasWidget.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "CanvasOptions.h"
#include "CanvasItemCommand.h"
#include "CanvasEventBridge.h"
#include "CanvasScrollBridge.h"
#include "CanvasBindCommand.h"
#include "D2DRenderer.h"
#include "WindowHook.h"
#include "CanvasPointerState.h"

namespace tkd2dcanvas {

namespace {

// Translates a pointer event into the "pick event" Tk uses for the current
// item: motion and button-release events are reported to item handlers as
// EnterNotify (Tk: the pickEvent in PickCurrentItem).
XEvent MakePickEvent(const XEvent* eventPtr) {
    XEvent pickEvent{};
    if (eventPtr->type == MotionNotify || eventPtr->type == ButtonRelease) {
        pickEvent.type = EnterNotify;
        pickEvent.xcrossing.type = EnterNotify;
        pickEvent.xcrossing.serial = eventPtr->xmotion.serial;
        pickEvent.xcrossing.send_event = eventPtr->xmotion.send_event;
        pickEvent.xcrossing.display = eventPtr->xmotion.display;
        pickEvent.xcrossing.window = eventPtr->xmotion.window;
        pickEvent.xcrossing.root = eventPtr->xmotion.root;
        pickEvent.xcrossing.subwindow = None;
        pickEvent.xcrossing.time = eventPtr->xmotion.time;
        pickEvent.xcrossing.x = eventPtr->xmotion.x;
        pickEvent.xcrossing.y = eventPtr->xmotion.y;
        pickEvent.xcrossing.x_root = eventPtr->xmotion.x_root;
        pickEvent.xcrossing.y_root = eventPtr->xmotion.y_root;
        pickEvent.xcrossing.mode = NotifyNormal;
        pickEvent.xcrossing.detail = NotifyNonlinear;
        pickEvent.xcrossing.same_screen = eventPtr->xmotion.same_screen;
        pickEvent.xcrossing.focus = False;
        pickEvent.xcrossing.state = eventPtr->xmotion.state;
    } else {
        pickEvent = *eventPtr;
    }
    return pickEvent;
}

// Evaluates "winfo <what> <path>" and returns the result as a string.
bool QueryWinfo(Tcl_Interp* interp, const std::string& path, const char* what, std::string& out) {
    Tcl_Obj* args[] = {
        Tcl_NewStringObj("winfo", -1),
        Tcl_NewStringObj(what, -1),
        Tcl_NewStringObj(path.c_str(), -1),
    };
    if (Tcl_EvalObjv(interp, 3, args, TCL_EVAL_GLOBAL) != TCL_OK) {
        Tcl_ResetResult(interp);
        return false;
    }
    out = Tcl_GetString(Tcl_GetObjResult(interp));
    return true;
}

int QueryInt(Tcl_Interp* interp, const std::string& path, const char* what, int fallback) {
    std::string text;
    if (!QueryWinfo(interp, path, what, text) || text.empty()) return fallback;
    return static_cast<int>(std::strtol(text.c_str(), nullptr, 10));
}

// "winfo id" returns the window id (the HWND on Windows) as a hex string.
HWND QueryHwnd(Tcl_Interp* interp, const std::string& path) {
    std::string id;
    if (!QueryWinfo(interp, path, "id", id) || id.empty()) return nullptr;
    const unsigned long long value = std::strtoull(id.c_str(), nullptr, 0);
    return reinterpret_cast<HWND>(static_cast<UINT_PTR>(value));
}

}  // namespace

CanvasWidget* CanvasWidget::Create(Tcl_Interp* interp, const char* pathName,
                                   int objc, Tcl_Obj* const objv[], int firstOption) {
    if (!pathName || pathName[0] != '.') {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("bad window path name \"%s\"",
                                                pathName ? pathName : ""));
        return nullptr;
    }

    Tk_Window parent = Tk_MainWindow(interp);
    if (!parent) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("no Tk main window", -1));
        return nullptr;
    }

    Tk_Window tkwin = Tk_CreateWindowFromPath(interp, parent, pathName, nullptr);
    if (!tkwin) {
        return nullptr;  // Tk already set the error message
    }

    auto widget = std::make_unique<CanvasWidget>();
    widget->interp = interp;
    widget->tkwin = tkwin;
    widget->path = pathName;

    widget->optionTable = CreateCanvasOptionTable(interp);
    if (!widget->optionTable) {
        CanvasWidget::AbortCreate(tkwin, *widget.release());
        return nullptr;
    }

    // The widget behaves like a Tk canvas for every tool that looks at the
    // class name (bindings, tkinter's winfo class, ...). The class has to be
    // set *before* the options are initialised: Tk looks the option database
    // up through the class hierarchy.
    Tk_SetClass(tkwin, "Canvas");

    if (Tk_InitOptions(interp, reinterpret_cast<char*>(widget.get()),
                        widget->optionTable, tkwin) != TCL_OK) {
        CanvasWidget::AbortCreate(tkwin, *widget.release());
        return nullptr;
    }

    int mask = 0;
    if (objc > firstOption) {
        if (Tk_SetOptions(interp, reinterpret_cast<char*>(widget.get()),
                           widget->optionTable, objc - firstOption, objv + firstOption,
                           tkwin, nullptr, &mask) != TCL_OK) {
            CanvasWidget::AbortCreate(tkwin, *widget.release());
            return nullptr;
        }
    }

    if (!widget->ApplyOptions(mask | kCanvasOptColour | kCanvasOptGeometry | kCanvasOptScroll)) {
        // A bad option value (e.g. -scrollregion) failed configuration
        CanvasWidget::AbortCreate(tkwin, *widget.release());
        return nullptr;
    }

    widget->command = Tcl_CreateObjCommand(interp, pathName, &CanvasWidget::WidgetCmd,
                                            widget.get(), &CanvasWidget::CommandDeleted);
    InstallEventHandlers(*widget);

    Tk_MakeWindowExist(tkwin);
    widget->UpdateGeometry();

    return widget.release();
}

void CanvasWidget::CommandDeleted(ClientData clientData) {
    auto* widget = static_cast<CanvasWidget*>(clientData);
    if (!widget) return;

    widget->RequestDestroy();
}

void CanvasWidget::RequestDestroy() {
    destroyPending = true;
    if (eventDepth > 0) return;  // freed by EndEvent()
    FinalizeDestroy();
}

void CanvasWidget::AbortCreate(Tk_Window tkwin, CanvasWidget& raw) {
    if (tkwin) {
        Tk_DestroyWindow(tkwin);
    }
    raw.FinalizeDestroy();
}

void CanvasWidget::FinalizeDestroy() {
    destroyPending = false;

    // A destroyed widget must not be touched by a pending idle callback.
    CancelScrollbarUpdate(*this);

    ShutdownWindowItems(*this);

    if (hwnd) {
        DetachCanvasFromWindow(hwnd, this);
        hwnd = nullptr;
    }
    if (tkwin && optionTable) {
        Tk_FreeConfigOptions(reinterpret_cast<char*>(this), optionTable, tkwin);
        optionTable = nullptr;
    }
    if (bindingTable) {
        Tk_DeleteBindingTable(bindingTable);
        bindingTable = nullptr;
    }
    for (auto& [id, key] : itemBindingObjects) {
        delete[] reinterpret_cast<char*>(key);
    }
    itemBindingObjects.clear();
    boundSequences.clear();
    virtualBindings.clear();
    surface.Detach();
    delete this;
}

void CanvasWidget::BeginEvent() {
    ++eventDepth;
}

void CanvasWidget::EndEvent() {
    if (--eventDepth <= 0) {
        eventDepth = 0;
        if (destroyPending) FinalizeDestroy();
    }
}

int CanvasWidget::WidgetCmd(ClientData clientData, Tcl_Interp* interp,
                            int objc, Tcl_Obj* const objv[]) {
    auto* widget = static_cast<CanvasWidget*>(clientData);
    if (!widget) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("unknown canvas widget", -1));
        return TCL_ERROR;
    }

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "subcommand ?arg ...?");
        return TCL_ERROR;
    }

    const std::string sub = Tcl_GetString(objv[1]);

    if (sub == "configure" || sub == "config") {
        if (objc == 2) {
            // No options given: report the current configuration.
            return CanvasReportOptions(interp, *widget);
        }
        int mask = 0;
        if (Tk_SetOptions(interp, reinterpret_cast<char*>(widget), widget->optionTable,
                           objc - 2, objv + 2, widget->tkwin, nullptr, &mask) != TCL_OK) {
            return TCL_ERROR;
        }
        if (!widget->ApplyOptions(mask)) {
            return TCL_ERROR;
        }
        Tcl_SetObjResult(interp, Tcl_NewStringObj(Tk_PathName(widget->tkwin), -1));
        return TCL_OK;
    }

    if (sub == "cget") {
        if (objc != 3) {
            Tcl_WrongNumArgs(interp, 1, objv, "cget option");
            return TCL_ERROR;
        }
        Tcl_Obj* value = Tk_GetOptionValue(interp, reinterpret_cast<char*>(widget),
                                            widget->optionTable, objv[2], widget->tkwin);
        if (!value) {
            return TCL_ERROR;  // Tk set the error message
        }
        Tcl_SetObjResult(interp, value);
        return TCL_OK;
    }

    return CanvasItemSubcommand(*widget, interp, objc, objv);
}

bool CanvasWidget::ApplyOptions(int mask) {
    return ApplyCanvasOptions(*this, mask);
}

void CanvasWidget::UpdateGeometry() {
    if (!interp || !tkwin) return;

    // Every Tk widget owns a Win32 child window on Windows; that is the
    // window we hook and paint into (coordinates are widget-local then).
    HWND hwnd = QueryHwnd(interp, path);
    if (hwnd != this->hwnd) {
        if (this->hwnd) DetachCanvasFromWindow(this->hwnd, this);
        this->hwnd = hwnd;
        if (hwnd) {
            AttachCanvasToWindow(hwnd, this);
            surface.AttachToWindow(hwnd);
        } else {
            surface.Detach();
        }
    }

    const int width = QueryInt(interp, path, "width", pixelWidth);
    const int height = QueryInt(interp, path, "height", pixelHeight);
    pixelWidth = width;
    pixelHeight = height;
    surface.Resize(width, height);

    model.View().SetViewSize(width, height);
    RequestScrollbarUpdate(*this);

    SyncWindowItems(*this);
}

void CanvasWidget::Invalidate() {
    if (!hwnd) return;
    InvalidateRect(hwnd, nullptr, FALSE);
}

void CanvasWidget::Invalidate(const BBox& box) {
    if (!box.IsValid()) return;
    const CanvasView& view = model.View();
    const BBox local{box.left - view.XOffset(), box.top - view.YOffset(),
                     box.right - view.XOffset(), box.bottom - view.YOffset()};
    surface.Invalidate(local);
}

void CanvasWidget::PickCurrentItem(int x, int y, XEvent* eventPtr) {
    const CanvasView& view = model.View();
    const XEvent pickEvent = eventPtr ? MakePickEvent(eventPtr) : XEvent{};

    if (repickDepth) return;

    const bool buttonDown = (buttonState & kAllButtonMasks) != 0;

    int newId = CanvasModel::kInvalidId;
    if (pickEvent.type != LeaveNotify) {
        newId = model.FindItemAt(x + view.XOffset(), y + view.YOffset(), closeEnough);
    }

    if (newId == currentItemId && !leftGrabbedItem) return;
    if (!buttonDown) leftGrabbedItem = false;

    // Simulate a LeaveNotify on the previous current item (Tk keeps the
    // "current" tag on the old item until the handler has run).
    if (newId != currentItemId && currentItemId != CanvasModel::kInvalidId &&
        !leftGrabbedItem) {
        const int oldId = currentItemId;
        const bool oldExists = model.Exists(oldId);
        if (oldExists && eventPtr) {
            XEvent ev = pickEvent;
            ev.type = LeaveNotify;
            ev.xcrossing.detail = NotifyAncestor;
            ++repickDepth;
            CanvasDispatchCrossing(*this, &ev, oldId);
            --repickDepth;
        }
        // A Leave handler may delete items (possibly the canvas itself).
        // With a button held the grabbed item keeps the "current" tag
        // (Tk: the tag is only removed when no button is down); the
        // release repick takes it away.
        if (!buttonDown && model.Exists(oldId)) model.RemoveTag(oldId, kCurrentTag);
        if (newId != CanvasModel::kInvalidId && !model.Exists(newId)) {
            newId = CanvasModel::kInvalidId;
        }
    }
    if (newId != currentItemId && buttonDown) {
        // A button is down: no new current item is entered until the
        // release (a form of pointer capture on the current item).
        leftGrabbedItem = true;
        return;
    }

    const int prevId = currentItemId;
    currentItemId = newId;
    if (newId != CanvasModel::kInvalidId) {
        model.AddTag(newId, kCurrentTag);
        if (eventPtr && prevId != newId) {
            XEvent ev = pickEvent;
            ev.type = EnterNotify;
            ev.xcrossing.detail = NotifyAncestor;
            CanvasDispatchCrossing(*this, &ev, newId);
        }
    }
}

void CanvasWidget::ClearCurrentItem() {
    if (currentItemId != CanvasModel::kInvalidId) {
        model.RemoveTag(currentItemId, kCurrentTag);
    }
    currentItemId = CanvasModel::kInvalidId;
    leftGrabbedItem = false;
}

std::vector<std::string> CanvasWidget::BoundSequences(ClientData object) const {
    const auto it = boundSequences.find(object);
    if (it == boundSequences.end()) return {};
    return it->second;
}

void CanvasWidget::RememberBinding(ClientData object, const std::string& sequence) {
    std::vector<std::string>& sequences = boundSequences[object];
    if (std::find(sequences.begin(), sequences.end(), sequence) == sequences.end()) {
        sequences.push_back(sequence);
    }
}

void CanvasWidget::ForgetBinding(ClientData object, const std::string& sequence) {
    const auto it = boundSequences.find(object);
    if (it == boundSequences.end()) return;
    auto& sequences = it->second;
    sequences.erase(std::remove(sequences.begin(), sequences.end(), sequence), sequences.end());
    if (sequences.empty()) boundSequences.erase(it);
}

void CanvasWidget::RememberVirtualBinding(ClientData object, const std::string& sequence,
                                           const std::string& script) {
    virtualBindings[object][sequence] = script;
}

std::string CanvasWidget::VirtualBinding(ClientData object, const std::string& sequence) const {
    const auto it = virtualBindings.find(object);
    if (it == virtualBindings.end()) return {};
    const auto script = it->second.find(sequence);
    if (script == it->second.end()) return {};
    return script->second;
}

void CanvasWidget::ForgetVirtualBinding(ClientData object, const std::string& sequence) {
    const auto it = virtualBindings.find(object);
    if (it == virtualBindings.end()) return;
    it->second.erase(sequence);
    if (it->second.empty()) virtualBindings.erase(it);
}

void CanvasWidget::Paint(HDC dc, const std::vector<RECT>& damaged) {
    if (!dc || !mapped) return;
    if (pixelWidth <= 0 || pixelHeight <= 0) return;

    // Only paint the parts of the damage that touch the widget.
    const RECT bounds{0, 0, pixelWidth, pixelHeight};
    std::vector<RECT> rects;
    rects.reserve(damaged.size());
    for (const RECT& rect : damaged) {
        RECT overlap{};
        if (IntersectRect(&overlap, &bounds, &rect)) rects.push_back(overlap);
    }
    if (rects.empty()) return;

    std::vector<std::pair<CanvasItem*, BBox>> items;
    items.reserve(model.Size());
    for (CanvasItem* item : model.ItemsInZOrder()) {
        if (!item) continue;
        items.emplace_back(item, item->GetBBox());
    }

    // The clip lives in Win32Surface::Paint (background fill + item draws are
    // clipped in D2D), and the same rectangles gate the BitBlt to the target
    // DC, so a small change repaints only that region.
    surface.Paint(dc, rects,
                  [this, &items](D2DRenderer& renderer, const RECT& overlap) {
                      const CanvasView& view = model.View();
                      const double dx = view.XOffset();
                      const double dy = view.YOffset();
                      renderer.pushTranslate(-dx, -dy);
                      constexpr double kCullSlack = 4.0;
                      const BBox visible{overlap.left + dx - kCullSlack,
                                         overlap.top + dy - kCullSlack,
                                         overlap.right + dx + kCullSlack,
                                         overlap.bottom + dy + kCullSlack};
                      for (const auto& entry : items) {
                          if (entry.first->IsHidden()) {
                               continue;
                           }
                          if (entry.second.IsValid() &&
                              !entry.second.Intersects(visible)) {
                              continue;
                          }
                          entry.first->Draw(renderer);
                      }
                      renderer.popTranslate();
                  });
}

void CanvasWidget::RepaintAfterScroll(double oldX, double oldY) {
    if (!hwnd) return;

    const CanvasView& view = model.View();
    const double newX = view.XOffset();
    const double newY = view.YOffset();
    const double deltaX = newX - oldX;
    const double deltaY = newY - oldY;

    if (deltaX == 0.0 && deltaY == 0.0) return;

    auto fallbackFullRepaint = [this]() {
        Invalidate();
        UpdateWindow(hwnd);
        SyncWindowItems(*this);
    };

    const double roundedX = std::round(deltaX);
    const double roundedY = std::round(deltaY);
    if (deltaX != roundedX || deltaY != roundedY) {
        fallbackFullRepaint();
        return;
    }

    const int dx = static_cast<int>(roundedX);
    const int dy = static_cast<int>(roundedY);
    if ((dx == 0 && dy == 0) ||
        std::abs(dx) >= pixelWidth || std::abs(dy) >= pixelHeight) {
        fallbackFullRepaint();
        return;
    }

    const std::vector<RECT> pending = Win32Surface::UpdateRegion(hwnd);

    long long pendingArea = 0;
    for (const RECT& rect : pending) {
        const long long w = static_cast<long long>(rect.right) - rect.left;
        const long long h = static_cast<long long>(rect.bottom) - rect.top;
        if (w > 0 && h > 0) pendingArea += w * h;
    }
    const long long windowArea = static_cast<long long>(pixelWidth) * static_cast<long long>(pixelHeight);
    if (windowArea <= 0 || pendingArea * 2 >= windowArea) {
        fallbackFullRepaint();
        return;
    }

    const int shiftX = -dx;
    const int shiftY = -dy;

    if (!surface.ScrollContent(shiftX, shiftY)) {
        fallbackFullRepaint();
        return;
    }

    if (ScrollWindowEx(hwnd, shiftX, shiftY, nullptr, nullptr, nullptr, nullptr,
                       SW_INVALIDATE) == ERROR) {
        fallbackFullRepaint();
        return;
    }

    // Repaint only the newly exposed strips. Each strip reaches one pixel
    // into the retained area so antialiased edges along the seam match the
    // shifted pixels.
    constexpr int kSeam = 1;
    if (shiftX < 0) {
        // Content moved left: the right edge is new.
        RECT strip{std::max(0, pixelWidth + shiftX - kSeam), 0, pixelWidth, pixelHeight};
        InvalidateRect(hwnd, &strip, FALSE);
    } else if (shiftX > 0) {
        RECT strip{0, 0, std::min(pixelWidth, shiftX + kSeam), pixelHeight};
        InvalidateRect(hwnd, &strip, FALSE);
    }
    if (shiftY < 0) {
        // Content moved up: the bottom edge is new.
        RECT strip{0, std::max(0, pixelHeight + shiftY - kSeam), pixelWidth, pixelHeight};
        InvalidateRect(hwnd, &strip, FALSE);
    } else if (shiftY > 0) {
        RECT strip{0, 0, pixelWidth, std::min(pixelHeight, shiftY + kSeam)};
        InvalidateRect(hwnd, &strip, FALSE);
    }

    for (const RECT& rect : pending) {
        const BBox box{rect.left + oldX, rect.top + oldY, rect.right - 1 + oldX, rect.bottom - 1 + oldY};
        Invalidate(box);
    }
    SyncWindowItems(*this);

    UpdateWindow(hwnd);
}

}  // namespace tkd2dcanvas
