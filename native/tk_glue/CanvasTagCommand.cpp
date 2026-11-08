#include "CanvasTagCommand.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "tkd2dcanvas/CanvasModel.h"

#include "CanvasItemCommand.h"
#include "CanvasWidget.h"

namespace tkd2dcanvas {

namespace {

// The search commands shared by "find" and "addtag" (Tk semantics).
enum class SearchKind {
    kWithTag,
    kAll,
    kAbove,
    kBelow,
    kClosest,
    kOverlapping,
    kEnclosed,
};

struct SearchCommand {
    const char* name;
    SearchKind kind;
};

constexpr SearchCommand kSearchCommands[] = {
    {"withtag", SearchKind::kWithTag},
    {"all", SearchKind::kAll},
    {"above", SearchKind::kAbove},
    {"below", SearchKind::kBelow},
    {"closest", SearchKind::kClosest},
    {"overlapping", SearchKind::kOverlapping},
    {"enclosed", SearchKind::kEnclosed},
};

bool ParseSearchKind(const char* name, SearchKind& out) {
    if (!name) return false;
    for (const SearchCommand& command : kSearchCommands) {
        if (std::strcmp(name, command.name) == 0) {
            out = command.kind;
            return true;
        }
    }
    return false;
}

const char* SearchKindName(SearchKind kind) {
    for (const SearchCommand& command : kSearchCommands) {
        if (command.kind == kind) return command.name;
    }
    return "withtag";
}

bool ParseCoord(Tcl_Interp* interp, Tcl_Obj* obj, double& out) {
    if (Tcl_GetDoubleFromObj(interp, obj, &out) != TCL_OK ||
        !std::isfinite(out)) {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("expected number but got \"%s\"",
                                                Tcl_GetString(obj)));
        return false;
    }
    return true;
}

// The item that sits directly above / below the given one in the display list.
int NeighbourInZOrder(CanvasWidget& widget, int id, bool above) {
    const std::vector<int> order = widget.model.AllItemIds();
    const auto it = std::find(order.begin(), order.end(), id);
    if (it == order.end()) return CanvasModel::kInvalidId;
    if (above) {
        if (it + 1 == order.end()) return CanvasModel::kInvalidId;
        return *(it + 1);
    }
    if (it == order.begin()) return CanvasModel::kInvalidId;
    return *(it - 1);
}

// Runs a Tk search command. `objv[first]` holds the search command name, the
// arguments follow. The resulting item ids are appended to `out` in display
// list order.
bool RunSearch(CanvasWidget& widget, Tcl_Interp* interp,
               int objc, Tcl_Obj* const objv[], int first,
               SearchKind kind, std::vector<int>& out) {
    const std::string name = SearchKindName(kind);

    switch (kind) {
        case SearchKind::kAll: {
            if (objc != first + 1) {
                Tcl_WrongNumArgs(interp, first + 1, objv, name.c_str());
                return false;
            }
            out = widget.model.AllItemIds();
            return true;
        }

        case SearchKind::kWithTag: {
            if (objc != first + 2) {
                Tcl_WrongNumArgs(interp, first + 1, objv, "withtag tagOrId");
                return false;
            }
            return ResolveItemSpecs(widget, interp, objv, first + 1, first + 2, out);
        }

        case SearchKind::kAbove:
        case SearchKind::kBelow: {
            if (objc != first + 2) {
                Tcl_WrongNumArgs(interp, first + 1, objv, "tagOrId");
                return false;
            }
            std::vector<int> matches;
            if (!ResolveItemSpecs(widget, interp, objv, first + 1, first + 2, matches)) {
                return false;
            }
            if (matches.empty()) return true;
            // "above" refers to the topmost match, "below" to the bottom-most.
            const int anchor = (kind == SearchKind::kAbove) ? matches.back() : matches.front();
            const int neighbour = NeighbourInZOrder(widget, anchor, kind == SearchKind::kAbove);
            if (neighbour != CanvasModel::kInvalidId) out.push_back(neighbour);
            return true;
        }

        case SearchKind::kClosest: {
            if (objc < first + 3 || objc > first + 5) {
                Tcl_WrongNumArgs(interp, first + 1, objv, "x y ?halo? ?start?");
                return false;
            }
            double x = 0.0, y = 0.0;
            if (!ParseCoord(interp, objv[first + 1], x)) return false;
            if (!ParseCoord(interp, objv[first + 2], y)) return false;
            double halo = 0.0;
            if (objc > first + 3 && !ParseCoord(interp, objv[first + 3], halo)) return false;
            if (halo < 0.0) {
                Tcl_SetObjResult(interp, Tcl_ObjPrintf(
                    "can't have negative halo value \"%f\"", halo));
                return false;
            }

            // The optional "start" argument makes the search start at that
            // item and wrap around the display list (Tk semantics).
            int startId = CanvasModel::kInvalidId;
            if (objc == first + 5) {
                std::vector<int> starts;
                if (!ResolveItemSpecs(widget, interp, objv, first + 4, first + 5, starts)) {
                    return false;
                }
                if (!starts.empty()) startId = starts.front();
            }

            const int closest = widget.model.FindClosestFrom(x, y, halo, startId);
            if (closest != CanvasModel::kInvalidId) out.push_back(closest);
            return true;
        }

        case SearchKind::kOverlapping:
        case SearchKind::kEnclosed: {
            if (objc != first + 5) {
                Tcl_WrongNumArgs(interp, first + 1, objv, "x1 y1 x2 y2");
                return false;
            }
            double rect[4];
            for (int i = 0; i < 4; ++i) {
                if (!ParseCoord(interp, objv[first + 1 + i], rect[i])) return false;
            }
            if (rect[0] > rect[2]) std::swap(rect[0], rect[2]);
            if (rect[1] > rect[3]) std::swap(rect[1], rect[3]);

            const bool enclosedOnly = (kind == SearchKind::kEnclosed);
            const int rx1 = static_cast<int>(rect[0] - 1.0);
            const int ry1 = static_cast<int>(rect[1] - 1.0);
            const int rx2 = static_cast<int>(rect[2] + 1.0);
            const int ry2 = static_cast<int>(rect[3] + 1.0);
            for (int id : widget.model.AllItemIds()) {
                const BBox box = widget.model.ItemBBox(id);
                if (!box.IsValid()) continue;
                CanvasItem* item = widget.model.FindItem(id);
                if (!item) continue;
                if (item->IsHidden()) continue;
                if (box.left >= rx2 || box.right <= rx1 || box.top >= ry2 || box.bottom <= ry1) {
                    continue;
                }
                const int overlap = item->AreaOverlap(rect);
                if (enclosedOnly ? (overlap == 1) : (overlap >= 0)) {
                    out.push_back(id);
                }
            }
            return true;
        }
    }
    return false;
}

// Dispatches a search command; `first` points at the command name.
bool Search(CanvasWidget& widget, Tcl_Interp* interp,
            int objc, Tcl_Obj* const objv[], int first,
            std::vector<int>& out) {
    const char* raw = Tcl_GetString(objv[first]);
    SearchKind kind = SearchKind::kWithTag;
    if (!ParseSearchKind(raw, kind)) {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf(
            "bad search command \"%s\": must be above, all, below, closest, "
            "enclosed, overlapping or withtag", raw ? raw : ""));
        return false;
    }
    return RunSearch(widget, interp, objc, objv, first, kind, out);
}

int FindCmd(CanvasWidget& widget, Tcl_Interp* interp,
            int objc, Tcl_Obj* const objv[]) {
    if (objc < 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "searchCommand ?arg ...?");
        return TCL_ERROR;
    }

    std::vector<int> ids;
    if (!Search(widget, interp, objc, objv, /*first=*/2, ids)) return TCL_ERROR;

    Tcl_Obj* list = Tcl_NewObj();
    for (int id : ids) Tcl_ListObjAppendElement(nullptr, list, Tcl_NewIntObj(id));
    Tcl_SetObjResult(interp, list);
    return TCL_OK;
}

int AddTagCmd(CanvasWidget& widget, Tcl_Interp* interp,
              int objc, Tcl_Obj* const objv[]) {
    if (objc < 4) {
        Tcl_WrongNumArgs(interp, 2, objv, "tag searchCommand ?arg ...?");
        return TCL_ERROR;
    }

    const char* tag = Tcl_GetString(objv[2]);
    if (!tag || !*tag) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("tag must not be empty", -1));
        return TCL_ERROR;
    }

    std::vector<int> ids;
    if (!Search(widget, interp, objc, objv, /*first=*/3, ids)) return TCL_ERROR;

    for (int id : ids) widget.model.AddTag(id, tag);
    widget.Invalidate();
    Tcl_SetObjResult(interp, Tcl_NewStringObj("", -1));
    return TCL_OK;
}

int DTagCmd(CanvasWidget& widget, Tcl_Interp* interp,
            int objc, Tcl_Obj* const objv[]) {
    if (objc != 3 && objc != 4) {
        Tcl_WrongNumArgs(interp, 2, objv, "tagOrId ?tagToDelete?");
        return TCL_ERROR;
    }

    // "dtag tag" removes the tag "tag" from every item that carries it;
    // "dtag item tag" removes "tag" from the matching item(s).
    const char* tagToDelete = (objc == 4) ? Tcl_GetString(objv[3]) : Tcl_GetString(objv[2]);

    std::vector<int> ids;
    if (!ResolveItemSpecs(widget, interp, objv, /*firstArg=*/2,
                          /*limit=*/3, ids)) {
        return TCL_ERROR;
    }

    for (int id : ids) widget.model.RemoveTag(id, tagToDelete);
    widget.Invalidate();
    Tcl_SetObjResult(interp, Tcl_NewStringObj("", -1));
    return TCL_OK;
}

int GetTagsCmd(CanvasWidget& widget, Tcl_Interp* interp,
               int objc, Tcl_Obj* const objv[]) {
    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "tagOrId");
        return TCL_ERROR;
    }

    std::vector<int> ids;
    if (!ResolveItemSpecs(widget, interp, objv, /*firstArg=*/2, /*limit=*/3, ids)) {
        return TCL_ERROR;
    }

    Tcl_Obj* list = Tcl_NewObj();
    if (!ids.empty()) {
        const CanvasItem* item = widget.model.FindItem(ids.front());
        if (item) {
            for (const std::string& tag : item->Tags()) {
                Tcl_ListObjAppendElement(nullptr, list, Tcl_NewStringObj(tag.c_str(), -1));
            }
        }
    }
    Tcl_SetObjResult(interp, list);
    return TCL_OK;
}

int RaiseLowerCmd(CanvasWidget& widget, Tcl_Interp* interp,
                  int objc, Tcl_Obj* const objv[], bool raise) {
    if (objc != 3 && objc != 4) {
        Tcl_WrongNumArgs(interp, 2, objv, raise ? "tagOrId ?aboveThis?" : "tagOrId ?belowThis?");
        return TCL_ERROR;
    }

    std::vector<int> ids;
    if (!ResolveItemSpecs(widget, interp, objv, /*firstArg=*/2, /*limit=*/3, ids)) {
        return TCL_ERROR;
    }

    int anchor = CanvasModel::kInvalidId;
    if (objc == 4) {
        std::vector<int> anchors;
        if (!ResolveItemSpecs(widget, interp, objv, /*firstArg=*/3, /*limit=*/4, anchors)) {
            return TCL_ERROR;
        }
        if (anchors.empty()) {
            Tcl_SetObjResult(interp, Tcl_ObjPrintf("tagOrId \"%s\" doesn't match any items", Tcl_GetString(objv[3])));
            return TCL_ERROR;
        }
        // "aboveThis" refers to the topmost match, "belowThis" to the
        // bottom-most one (Tk semantics).
        anchor = raise ? anchors.back() : anchors.front();
    }

    if (raise) {
        widget.model.MoveItemsAbove(ids, anchor);
    } else {
        widget.model.MoveItemsBelow(ids, anchor);
    }
    widget.Invalidate();
    Tcl_SetObjResult(interp, Tcl_NewStringObj("", -1));
    return TCL_OK;
}

}  // namespace

int CanvasTagSubcommand(CanvasWidget& widget, Tcl_Interp* interp,
                        int objc, Tcl_Obj* const objv[]) {
    const std::string cmd = (objc >= 2) ? Tcl_GetString(objv[1]) : "";

    if (cmd == "find") {
        return FindCmd(widget, interp, objc, objv);
    }
    if (cmd == "addtag") {
        return AddTagCmd(widget, interp, objc, objv);
    }
    if (cmd == "dtag") {
        return DTagCmd(widget, interp, objc, objv);
    }
    if (cmd == "gettags") {
        return GetTagsCmd(widget, interp, objc, objv);
    }
    if (cmd == "raise" || cmd == "tagraise") {
        return RaiseLowerCmd(widget, interp, objc, objv, /*raise=*/true);
    }
    if (cmd == "lower" || cmd == "taglower") {
        return RaiseLowerCmd(widget, interp, objc, objv, /*raise=*/false);
    }

    // The Tk 4 spellings of the search commands.
    if (cmd == "findwithtag" || cmd == "findall" || cmd == "findclosest") {
        Tcl_Obj* shifted[6] = {nullptr};
        int count = 0;
        shifted[count++] = objv[0];
        shifted[count++] = Tcl_NewStringObj("find", -1);
        if (cmd == "findwithtag") {
            if (objc != 3) {
                Tcl_WrongNumArgs(interp, 2, objv, "tagOrId");
                return TCL_ERROR;
            }
            shifted[count++] = Tcl_NewStringObj("withtag", -1);
            shifted[count++] = objv[2];
        } else if (cmd == "findall") {
            if (objc != 2) {
                Tcl_WrongNumArgs(interp, 2, objv, nullptr);
                return TCL_ERROR;
            }
            shifted[count++] = Tcl_NewStringObj("all", -1);
        } else {
            if (objc < 3 || objc > 4) {
                Tcl_WrongNumArgs(interp, 2, objv, "x y ?halo?");
                return TCL_ERROR;
            }
            shifted[count++] = Tcl_NewStringObj("closest", -1);
            shifted[count++] = objv[2];
            shifted[count++] = objv[3];
            if (objc == 4) shifted[count++] = objv[4];
        }
        return FindCmd(widget, interp, count, shifted);
    }

    Tcl_SetObjResult(interp, Tcl_ObjPrintf("unknown widget subcommand \"%s\"", cmd.c_str()));
    return TCL_ERROR;
}

}  // namespace tkd2dcanvas
