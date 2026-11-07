#include "CanvasWidget.h"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "CanvasOptions.h"
#include "CanvasItemCommand.h"
#include "CanvasEventBridge.h"
#include "CanvasScrollBridge.h"
#include "D2DRenderer.h"
#include "WindowHook.h"

namespace tkd2dcanvas {

namespace {

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

void CanvasWidget::PickCurrentItem(int x, int y) {
    // The hit test lives in the C++ core: the topmost item whose shape is
    // within the -closeenough tolerance wins. Widget-local coordinates are
    // mapped to canvas coordinates with the view offset.
    const CanvasView& view = model.View();
    const int id = model.FindItemAt(x + view.XOffset(), y + view.YOffset(), closeEnough);
    if (id == currentItemId) return;

    // The "current" tag follows the item under the pointer (Tk semantics).
    if (currentItemId != CanvasModel::kInvalidId) {
        model.RemoveTag(currentItemId, kCurrentTag);
    }
    if (id != CanvasModel::kInvalidId) {
        model.AddTag(id, kCurrentTag);
    }
    currentItemId = id;
}

void CanvasWidget::ClearCurrentItem() {
    if (currentItemId != CanvasModel::kInvalidId) {
        model.RemoveTag(currentItemId, kCurrentTag);
    }
    currentItemId = CanvasModel::kInvalidId;
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

void CanvasWidget::Paint(HDC dc, const RECT& damaged) {
    if (!dc || !mapped) return;
    if (pixelWidth <= 0 || pixelHeight <= 0) return;

    // Only paint when the damaged area touches the widget.
    RECT bounds{0, 0, pixelWidth, pixelHeight};
    RECT overlap{};
    if (!IntersectRect(&overlap, &bounds, &damaged)) return;

    // The clip lives in Win32Surface::Paint (background fill + item draws are
    // clipped in D2D), and the same rect gates the BitBlt to the target DC,
    // so a small change repaints only that region.
    surface.Paint(dc, &overlap,
                  [this, &overlap](D2DRenderer& renderer) {
                      const CanvasView& view = model.View();
                      const double dx = view.XOffset();
                      const double dy = view.YOffset();
                      renderer.pushTranslate(-dx, -dy);
                      constexpr double kCullSlack = 4.0;
                      const BBox visible{overlap.left + dx - kCullSlack,
                                         overlap.top + dy - kCullSlack,
                                         overlap.right + dx + kCullSlack,
                                         overlap.bottom + dy + kCullSlack};
                      for (CanvasItem* item : model.ItemsInZOrder()) {
                          if (!item) continue;
                          const BBox box = item->GetBBox();
                          if (box.IsValid() && !box.Intersects(visible)) continue;
                          item->Draw(renderer);
                      }
                      renderer.popTranslate();
                  });
}

}  // namespace tkd2dcanvas
