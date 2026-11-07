#include "CanvasItemCommand.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "tkd2dcanvas/CanvasItem.h"
#include "tkd2dcanvas/CanvasModel.h"
#include "tkd2dcanvas/ItemFactory.h"

#include "CanvasItemOptions.h"
#include "CanvasBindCommand.h"
#include "CanvasTagCommand.h"
#include "CanvasWidget.h"

namespace tkd2dcanvas {

namespace {

std::string FormatDouble(double value) {
    if (value == 0.0) return "0";  // normalizes -0
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.17g", value);
    return buf;
}

std::string FormatCoordsList(const std::vector<double>& coords) {
    std::string out;
    for (std::size_t i = 0; i < coords.size(); ++i) {
        if (i != 0) out += ' ';
        out += FormatDouble(coords[i]);
    }
    return out;
}

// Invalidates the region covered by an item, inflated for stroke width and
// antialiasing.
void InvalidatePadded(CanvasWidget& widget, const BBox& box, double width) {
    if (!box.IsValid()) return;
    BBox padded = box;
    const double pad = width * 0.5 + 2.0;
    padded.Inflate(pad, pad);
    widget.Invalidate(padded);
}

void InvalidateItem(CanvasWidget& widget, const CanvasItem& item) {
    InvalidatePadded(widget, item.GetBBox(), item.Style().width);
}

// Invalidates around an item using `padWidth` as the stroke width.
void InvalidateItem(CanvasWidget& widget, const CanvasItem& item, double padWidth) {
    InvalidatePadded(widget, item.GetBBox(), padWidth);
}

// Resolves Tk-style item specs (item id / "all" / tag name) in
// objv[firstArg, limit) to a deduplicated list of existing item ids.
// A purely numeric spec that does not name an existing item selects
// nothing and is *not* an error (Tk semantics: `move 9999 1 1` and
// friends are silent no-ops); a tag matching nothing is *not* either.
static bool ResolveItemSpecsImpl(CanvasWidget& widget, Tcl_Interp* interp,
                                 Tcl_Obj* const objv[], int firstArg, int limit,
                                 std::vector<int>& out) {
    out.clear();
    auto append = [&](int id) {
        if (std::find(out.begin(), out.end(), id) == out.end()) out.push_back(id);
    };

    for (int i = firstArg; i < limit; ++i) {
        const char* spec = Tcl_GetString(objv[i]);

        bool digits = (*spec != '\0');
        for (const char* p = spec; *p; ++p) {
            if (*p < '0' || *p > '9') { digits = false; break; }
        }

        if (digits) {
            long value = 0;
            if (Tcl_GetLongFromObj(nullptr, objv[i], &value) == TCL_OK &&
                value >= 0 && value <= INT_MAX &&
                widget.model.Exists(static_cast<int>(value))) {
                append(static_cast<int>(value));
            }
        } else if (std::strcmp(spec, "all") == 0) {
            for (int id : widget.model.AllItemIds()) append(id);
        } else {
            for (int id : widget.model.FindWithTag(spec)) append(id);
        }
    }

    // Sort by z-order: tag lookups come out of an unordered multimap, and
    // query commands take the *first* matching item, so the order must be
    // deterministic.
    const std::vector<int> zOrder = widget.model.AllItemIds();
    std::unordered_map<int, int> position;
    position.reserve(zOrder.size());
    for (int i = 0; i < static_cast<int>(zOrder.size()); ++i) position.emplace(zOrder[i], i);
    std::stable_sort(out.begin(), out.end(), [&](int a, int b) { return position[a] < position[b]; });
    return true;
}

// Reads a coordinate list starting at `first`: either all remaining arguments
// parsed as doubles, or (when exactly one argument remains) a Tcl list of
// doubles. Returns false with the interpreter error set on bad input.
bool ParseCoordinateArgs(Tcl_Interp* interp, int objc, Tcl_Obj* const objv[],
                         int first, std::vector<double>& coords) {
    coords.clear();

    auto pushNumber = [&](Tcl_Obj* obj) -> bool {
        double value = 0.0;
        if (Tcl_GetDoubleFromObj(interp, obj, &value) != TCL_OK) {
            Tcl_SetObjResult(interp, Tcl_ObjPrintf("expected number but got \"%s\"", Tcl_GetString(obj)));
            return false;
        }
        coords.push_back(value);
        return true;
    };

    if (objc - first == 1) {
        Tcl_Obj* only = objv[first];
        int length = 0;
        if (Tcl_ListObjLength(nullptr, only, &length) == TCL_OK && length > 1) {
            for (int j = 0; j < length; ++j) {
                Tcl_Obj* element = nullptr;
                if (Tcl_ListObjIndex(nullptr, only, j, &element) != TCL_OK || !element) {
                    Tcl_SetObjResult(interp, Tcl_NewStringObj("bad coordinate list", -1));
                    coords.clear();
                    return false;
                }
                double value = 0.0;
                if (Tcl_GetDoubleFromObj(interp, element, &value) != TCL_OK) {
                    Tcl_SetObjResult(interp, Tcl_ObjPrintf("expected number but got \"%s\"", Tcl_GetString(element)));
                    coords.clear();
                    return false;
                }
                coords.push_back(value);
            }
            return true;
        }
        // A single non-list argument: parse it as one coordinate.
        Tcl_ResetResult(interp);
        return pushNumber(only);
    }

    for (int i = first; i < objc; ++i) {
        if (!pushNumber(objv[i])) return false;
    }
    return true;
}

int CreateItemCmd(CanvasWidget& widget, Tcl_Interp* interp,
                  int objc, Tcl_Obj* const objv[]) {
    if (objc < 4) {
        Tcl_WrongNumArgs(interp, 2, objv, "itemType coord ... ?-option value ...?");
        return TCL_ERROR;
    }
    const char* type = Tcl_GetString(objv[2]);

    // Coordinates are the leading run of numeric arguments; the first
    // non-number must start the option list.
    std::vector<double> coords;
    int index = 3;
    while (index < objc) {
        double value = 0.0;
        if (Tcl_GetDoubleFromObj(interp, objv[index], &value) == TCL_OK) {
            coords.push_back(value);
            ++index;
            continue;
        }
        Tcl_ResetResult(interp);  // discard the failed-parse error message
        const char* token = Tcl_GetString(objv[index]);
        if (token[0] == '-') break;  // start of the option list
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("expected number but got \"%s\"", token));
        return TCL_ERROR;
    }

    CanvasItemPtr item;
    try {
        item = ItemFactory::Instance().Create(type, /*id=*/0, coords);
    } catch (const std::invalid_argument& e) {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("%s", e.what()));
        return TCL_ERROR;
    }

    const int id = widget.model.CreateItem(std::move(item));
    CanvasItem* created = widget.model.FindItem(id);  // never null at this point

    // Remaining arguments are "-option value" pairs. Parse + check first
    // (nothing is applied), then apply: a bad option deletes the half-item.
    ItemConfig config;
    if (!ParseItemConfig(interp, widget, /*firstArg=*/index, objc, objv, config) ||
        !CheckItemConfig(interp, *created, config)) {
        widget.model.DeleteItem(id);
        return TCL_ERROR;
    }
    ApplyItemConfig(widget, id, *created, config);

    InvalidateItem(widget, *created);
    Tcl_SetObjResult(interp, Tcl_NewIntObj(id));
    return TCL_OK;
}

int CoordsCmd(CanvasWidget& widget, Tcl_Interp* interp,
              int objc, Tcl_Obj* const objv[]) {
    if (objc < 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "coords tagOrId ?x y ...?");
        return TCL_ERROR;
    }

    std::vector<int> ids;
    if (!ResolveItemSpecs(widget, interp, objv, /*firstArg=*/2, /*limit=*/3, ids)) {
        return TCL_ERROR;
    }

    if (objc == 3) {
        // Query: report the coordinates of the first matching item.
        if (ids.empty()) {
            Tcl_SetObjResult(interp, Tcl_NewStringObj("", -1));
            return TCL_OK;
        }
        CanvasItem* item = widget.model.FindItem(ids.front());
        if (!item) {
            Tcl_SetObjResult(interp, Tcl_NewStringObj("item disappeared", -1));
            return TCL_ERROR;
        }
        Tcl_SetObjResult(interp, Tcl_NewStringObj(FormatCoordsList(item->GetCoordinates()).c_str(), -1));
        return TCL_OK;
    }

    if (ids.empty()) return TCL_OK;  // setting coords of an empty tag: no-op

    std::vector<double> coords;
    if (!ParseCoordinateArgs(interp, objc, objv, /*first=*/3, coords)) {
        return TCL_ERROR;
    }
    if (coords.size() < 2 || coords.size() % 2 != 0) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("expected an even number of coordinates", -1));
        return TCL_ERROR;
    }

    // Apply to all matching items atomically: on failure roll back the
    // items that were already updated.
    struct Backup {
        CanvasItem* item;
        std::vector<double> oldCoords;
        BBox oldBox;
    };
    std::vector<Backup> applied;
    for (int id : ids) {
        CanvasItem* item = widget.model.FindItem(id);
        if (!item) continue;
        Backup backup{item, item->GetCoordinates(), item->GetBBox()};
        if (!item->SetCoordinates(coords)) {
            for (const Backup& done : applied) done.item->SetCoordinates(done.oldCoords);
            Tcl_SetObjResult(interp, Tcl_ObjPrintf("wrong # coordinates: expected %d, got %d",
                                static_cast<int>(backup.oldCoords.size()),
                                static_cast<int>(coords.size())));
            return TCL_ERROR;
        }
        applied.push_back(std::move(backup));
    }

    for (const Backup& backup : applied) {
        InvalidatePadded(widget, backup.oldBox, backup.item->Style().width);
        InvalidateItem(widget, *backup.item);
    }    Tcl_SetObjResult(interp, Tcl_NewStringObj("", -1));
    return TCL_OK;
}

int MoveCmd(CanvasWidget& widget, Tcl_Interp* interp,
            int objc, Tcl_Obj* const objv[]) {
    if (objc != 5) {
        Tcl_WrongNumArgs(interp, 2, objv, "move tagOrId dx dy");
        return TCL_ERROR;
    }

    double dx = 0.0, dy = 0.0;
    if (Tcl_GetDoubleFromObj(interp, objv[3], &dx) != TCL_OK) {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("expected number but got \"%s\"", Tcl_GetString(objv[3])));
        return TCL_ERROR;
    }
    if (Tcl_GetDoubleFromObj(interp, objv[4], &dy) != TCL_OK) {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("expected number but got \"%s\"", Tcl_GetString(objv[4])));
        return TCL_ERROR;
    }

    std::vector<int> ids;
    if (!ResolveItemSpecs(widget, interp, objv, /*firstArg=*/2, /*limit=*/3, ids)) {
        return TCL_ERROR;
    }

    for (int id : ids) {
        CanvasItem* item = widget.model.FindItem(id);
        if (!item) continue;
        const BBox before = item->GetBBox();
        item->Translate(dx, dy);
        InvalidatePadded(widget, before, item->Style().width);
        InvalidateItem(widget, *item);
    }
    Tcl_SetObjResult(interp, Tcl_NewStringObj("", -1));
    return TCL_OK;
}

int MoveToCmd(CanvasWidget& widget, Tcl_Interp* interp,
              int objc, Tcl_Obj* const objv[]) {
    if (objc != 5) {
        Tcl_WrongNumArgs(interp, 2, objv, "moveto tagOrId x y");
        return TCL_ERROR;
    }

    // Tk: x and y may be the empty string, in which case the corresponding
    // coordinate is unchanged.
    double newX = 0.0, newY = 0.0;
    const bool xBlank = (Tcl_GetString(objv[3])[0] == '\0');
    const bool yBlank = (Tcl_GetString(objv[4])[0] == '\0');
    if (!xBlank && Tcl_GetDoubleFromObj(interp, objv[3], &newX) != TCL_OK) {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("expected number but got \"%s\"", Tcl_GetString(objv[3])));
        return TCL_ERROR;
    }
    if (!yBlank && Tcl_GetDoubleFromObj(interp, objv[4], &newY) != TCL_OK) {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("expected number but got \"%s\"", Tcl_GetString(objv[4])));
        return TCL_ERROR;
    }

    std::vector<int> ids;
    if (!ResolveItemSpecs(widget, interp, objv, /*firstArg=*/2, /*limit=*/3, ids)) {
        return TCL_ERROR;
    }
    if (ids.empty()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("", -1));
        return TCL_OK;
    }

    const BBox ref = widget.model.ItemBBox(ids.front());
    const double dx = xBlank ? 0.0 : newX - ref.left;
    const double dy = yBlank ? 0.0 : newY - ref.top;

    for (int id : ids) {
        CanvasItem* item = widget.model.FindItem(id);
        if (!item) continue;
        const BBox before = item->GetBBox();
        item->Translate(dx, dy);
        InvalidatePadded(widget, before, item->Style().width);
        InvalidateItem(widget, *item);
    }
    Tcl_SetObjResult(interp, Tcl_NewStringObj("", -1));
    return TCL_OK;
}

int DeleteCmd(CanvasWidget& widget, Tcl_Interp* interp,
              int objc, Tcl_Obj* const objv[]) {
    if (objc < 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "delete tagOrId ?tagOrId ...?");
        return TCL_ERROR;
    }

    std::vector<int> ids;
    if (!ResolveItemSpecs(widget, interp, objv, /*firstArg=*/2,
                          /*limit=*/objc, ids)) {
        return TCL_ERROR;
    }

    for (int id : ids) {
        CanvasItem* item = widget.model.FindItem(id);
        if (!item) continue;
        const double width = item->Style().width;
        const BBox box = item->GetBBox();
        widget.model.DeleteItem(id);
        if (widget.currentItemId == id) widget.ClearCurrentItem();
        InvalidatePadded(widget, box, width);
    }
    Tcl_SetObjResult(interp, Tcl_NewStringObj("", -1));
    return TCL_OK;
}

int BBoxCmd(CanvasWidget& widget, Tcl_Interp* interp,
            int objc, Tcl_Obj* const objv[]) {
    if (objc < 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "bbox tagOrId ?tagOrId ...?");
        return TCL_ERROR;
    }

    std::vector<int> ids;
    if (!ResolveItemSpecs(widget, interp, objv, /*firstArg=*/2,
                          /*limit=*/objc, ids)) {
        return TCL_ERROR;
    }

    BBox unionBox = BBox::Empty();
    bool any = false;
    for (int id : ids) {
        const BBox box = widget.model.ItemBBox(id);
        if (box.left >= box.right || box.top >= box.bottom) continue;
        unionBox = unionBox.Union(box);  // Empty unions correctly
        any = true;
    }

    if (!any) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("", -1));  // Tk returns ""
        return TCL_OK;
    }

    const std::string text = FormatCoordsList({unionBox.left, unionBox.top, unionBox.right, unionBox.bottom});
    Tcl_SetObjResult(interp, Tcl_NewStringObj(text.c_str(), -1));
    return TCL_OK;
}

int ItemConfigureCmd(CanvasWidget& widget, Tcl_Interp* interp,
                     int objc, Tcl_Obj* const objv[]) {
    if (objc < 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "itemconfigure tagOrId ?-option value? ...");
        return TCL_ERROR;
    }

    std::vector<int> ids;
    if (!ResolveItemSpecs(widget, interp, objv, /*firstArg=*/2, /*limit=*/3, ids)) {
        return TCL_ERROR;
    }

    if (objc == 3) {
        // Report mode: dict of the first matching item's options.
        if (ids.empty()) {
            Tcl_SetObjResult(interp, Tcl_NewStringObj("", -1));
            return TCL_OK;
        }
        CanvasItem* item = widget.model.FindItem(ids.front());
        if (!item) return TCL_ERROR;
        Tcl_SetObjResult(interp, ItemConfigReport(interp, *item));
        return TCL_OK;
    }

    ItemConfig config;
    if (!ParseItemConfig(interp, widget, /*firstArg=*/3, objc, objv, config)) {
        return TCL_ERROR;  // nothing applied anywhere
    }
    for (int id : ids) {
        CanvasItem* item = widget.model.FindItem(id);
        if (!item) continue;
        if (!CheckItemConfig(interp, *item, config)) return TCL_ERROR;
    }
    for (int id : ids) {
        CanvasItem* item = widget.model.FindItem(id);
        if (!item) continue;
        const double oldWidth = item->Style().width;
        ApplyItemConfig(widget, id, *item, config);
        InvalidateItem(widget, *item, std::max(oldWidth, item->Style().width));
    }
    Tcl_SetObjResult(interp, Tcl_NewStringObj("", -1));
    return TCL_OK;
}

int ItemCgetCmd(CanvasWidget& widget, Tcl_Interp* interp,
                int objc, Tcl_Obj* const objv[]) {
    if (objc != 4) {
        Tcl_WrongNumArgs(interp, 2, objv, "itemcget tagOrId option");
        return TCL_ERROR;
    }

    // Normalize the option name (leading dash stripped, lowercased).
    const char* raw = Tcl_GetString(objv[3]);
    std::string name = raw ? raw : "";
    if (!name.empty() && name.front() == '-') name.erase(name.begin());
    for (auto& c : name) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    static const char* kOptions[] = {"fill", "outline", "width", "dash", "dashoffset", "tags",
                                     "start", "extent", "style", "joinstyle", "capstyle",
                                     "smooth", "splinesteps"};
    bool known = false;
    for (const char* option : kOptions) {
        if (name == option) { known = true; break; }
    }
    if (!known) {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("unknown option \"%s\"", raw));
        return TCL_ERROR;
    }

    std::vector<int> ids;
    if (!ResolveItemSpecs(widget, interp, objv, /*firstArg=*/2, /*limit=*/3, ids)) {
        return TCL_ERROR;
    }
    if (ids.empty()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("", -1));
        return TCL_OK;
    }
    CanvasItem* item = widget.model.FindItem(ids.front());
    if (!item) return TCL_ERROR;

    const bool arcOnly = (name == "start" || name == "extent" || name == "style");
    if (arcOnly && item->Type() != "arc") {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("option \"-%s\" is not valid for %s items", name.c_str(), item->Type().c_str()));
        return TCL_ERROR;
    }
    if (name == "joinstyle" &&
        item->Type() != "line" && item->Type() != "polygon") {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("option \"-joinstyle\" is not valid for %s items", item->Type().c_str()));
        return TCL_ERROR;
    }
    if (name == "capstyle" && item->Type() != "line") {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("option \"-capstyle\" is not valid for %s items", item->Type().c_str()));
        return TCL_ERROR;
    }
    if ((name == "smooth" || name == "splinesteps") &&
        item->Type() != "line" && item->Type() != "polygon") {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("option \"-%s\" is not valid for %s items", name.c_str(), item->Type().c_str()));
        return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, Tcl_NewStringObj(ItemOptionValueString(*item, name.c_str()).c_str(), -1));
    return TCL_OK;
}

}  // namespace

bool ResolveItemSpecs(CanvasWidget& widget, Tcl_Interp* interp,
                      Tcl_Obj* const objv[], int firstArg, int limit,
                      std::vector<int>& out) {
    return ResolveItemSpecsImpl(widget, interp, objv, firstArg, limit, out);
}

int CanvasItemSubcommand(CanvasWidget& widget, Tcl_Interp* interp,
                         int objc, Tcl_Obj* const objv[]) {
    const std::string cmd = (objc >= 2) ? Tcl_GetString(objv[1]) : "";

    if (cmd == "create") {
        return CreateItemCmd(widget, interp, objc, objv);
    }
    if (cmd == "coords") {
        return CoordsCmd(widget, interp, objc, objv);
    }
    if (cmd == "move") {
        return MoveCmd(widget, interp, objc, objv);
    }
    if (cmd == "moveto") {
        return MoveToCmd(widget, interp, objc, objv);
    }
    if (cmd == "delete") {
        return DeleteCmd(widget, interp, objc, objv);
    }
    if (cmd == "bbox") {
        return BBoxCmd(widget, interp, objc, objv);
    }
    if (cmd == "itemconfigure" || cmd == "itemconfig") {
        return ItemConfigureCmd(widget, interp, objc, objv);
    }
    if (cmd == "itemcget") {
        return ItemCgetCmd(widget, interp, objc, objv);
    }
    if (cmd == "addtag" || cmd == "dtag" || cmd == "gettags") {
        return CanvasTagSubcommand(widget, interp, objc, objv);
    }
    if (cmd == "find" || cmd == "findwithtag" || cmd == "findall" || cmd == "findclosest") {
        return CanvasTagSubcommand(widget, interp, objc, objv);
    }
    if (cmd == "bind" || cmd == "tagbind") {
        return CanvasBindSubcommand(widget, interp, objc, objv);
    }
    if (cmd == "raise" || cmd == "lower" || cmd == "tagraise" || cmd == "taglower") {
        return CanvasTagSubcommand(widget, interp, objc, objv);
    }
    if (cmd == "xview" || cmd == "yview") {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(
            "tkd2dcanvas: xview/yview is not implemented yet (milestone M7)", -1));
        return TCL_ERROR;
    }
    if (cmd == "scan" || cmd == "scanmark" || cmd == "scandragto") {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(
            "tkd2dcanvas: scan is not implemented yet (milestone M7)", -1));
        return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, Tcl_ObjPrintf("unknown widget subcommand \"%s\"", cmd.c_str()));
    return TCL_ERROR;
}

}  // namespace tkd2dcanvas
