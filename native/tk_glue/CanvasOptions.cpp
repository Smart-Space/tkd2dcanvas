#include "CanvasOptions.h"

#include <string>

#include "CanvasTkColor.h"
#include "CanvasWidget.h"

namespace tkd2dcanvas {

namespace {

// Widget level options. The layout mirrors Tk's own widgets (see e.g.
// tkCanvas.c / tkFrame.c): standard window options plus the canvas specific
// ones. Values are stored directly in the CanvasWidget record.
const Tk_OptionSpec kCanvasOptionSpecs[] = {
    {TK_OPTION_COLOR, "-background", "background", "Background",
     (char *) "white", Tk_Offset(CanvasWidget, backgroundObj), -1,
     TK_OPTION_NULL_OK, (char *) NULL, kCanvasOptColour},
    {TK_OPTION_SYNONYM, "-bg", NULL, NULL,
     NULL, 0, -1, 0, "-background", 0},
    {TK_OPTION_PIXELS, "-borderwidth", "borderWidth", "BorderWidth",
     (char *) "0", -1, Tk_Offset(CanvasWidget, borderWidth),
     0, (char *) NULL, kCanvasOptGeometry},
    {TK_OPTION_SYNONYM, "-bd", NULL, NULL,
     NULL, 0, -1, 0, "-borderwidth", 0},
    {TK_OPTION_STRING, "-class", "class", "Class",
     (char *) "Canvas", Tk_Offset(CanvasWidget, classObj),
     -1, 0, (char *) NULL, kCanvasOptOther},
    {TK_OPTION_DOUBLE, "-closeenough", "closeEnough", "CloseEnough",
     (char *) "1.0", -1, Tk_Offset(CanvasWidget, closeEnough),
     0, (char *) NULL, kCanvasOptOther},
    {TK_OPTION_BOOLEAN, "-confine", "confine", "Confine",
     (char *) "1", -1, Tk_Offset(CanvasWidget, confine),
     0, (char *) NULL, kCanvasOptScroll},
    {TK_OPTION_CURSOR, "-cursor", "cursor", "Cursor",
     (char *) "", Tk_Offset(CanvasWidget, cursorObj),
     -1, TK_OPTION_NULL_OK, (char *) NULL, kCanvasOptOther},
    {TK_OPTION_PIXELS, "-height", "height", "Height",
     (char *) "300", -1, Tk_Offset(CanvasWidget, height),
     0, (char *) NULL, kCanvasOptGeometry},
    {TK_OPTION_PIXELS, "-highlightthickness", "highlightThickness",
     "HighlightThickness", (char *) "0", -1,
     Tk_Offset(CanvasWidget, highlightWidth), 0, (char *) NULL, kCanvasOptOther},
    {TK_OPTION_COLOR, "-highlightbackground", "highlightBackground",
     "HighlightBackground", (char *) "#000000",
     Tk_Offset(CanvasWidget, highlightBackgroundObj), -1,
     TK_OPTION_NULL_OK, (char *) NULL, kCanvasOptOther},
    {TK_OPTION_COLOR, "-highlightcolor", "highlightColor", "HighlightColor",
     (char *) "#000000", Tk_Offset(CanvasWidget, highlightColorObj), -1,
     TK_OPTION_NULL_OK, (char *) NULL, kCanvasOptOther},
    {TK_OPTION_COLOR, "-insertbackground", "insertBackground", "Foreground",
     (char *) "#000000", Tk_Offset(CanvasWidget, insertBackgroundObj), -1,
     TK_OPTION_NULL_OK, (char *) NULL, kCanvasOptOther},
    {TK_OPTION_PIXELS, "-insertborderwidth", "insertBorderWidth",
     "BorderWidth", (char *) "0", -1,
     Tk_Offset(CanvasWidget, insertBorderWidth), 0, (char *) NULL, kCanvasOptOther},
    {TK_OPTION_PIXELS, "-insertwidth", "insertWidth", "InsertWidth",
     (char *) "2", -1, Tk_Offset(CanvasWidget, insertWidth), 0,
     (char *) NULL, kCanvasOptOther},
    {TK_OPTION_RELIEF, "-relief", "relief", "Relief",
     (char *) "flat", -1, Tk_Offset(CanvasWidget, relief),
     0, (char *) NULL, kCanvasOptOther},
    {TK_OPTION_COLOR, "-selectbackground", "selectBackground", "Foreground",
     (char *) "#000000", Tk_Offset(CanvasWidget, selectBackgroundObj), -1,
     TK_OPTION_NULL_OK, (char *) NULL, kCanvasOptOther},
    {TK_OPTION_PIXELS, "-selectborderwidth", "selectBorderWidth",
     "BorderWidth", (char *) "0", -1,
     Tk_Offset(CanvasWidget, selectBorderWidth), 0, (char *) NULL, kCanvasOptOther},
    {TK_OPTION_COLOR, "-selectforeground", "selectForeground", "Background",
     (char *) "#000000", Tk_Offset(CanvasWidget, selectForegroundObj), -1,
     TK_OPTION_NULL_OK, (char *) NULL, kCanvasOptOther},
    {TK_OPTION_STRING, "-scrollregion", "scrollRegion", "ScrollRegion",
     (char *) "", Tk_Offset(CanvasWidget, scrollRegionObj),
     -1, 0, (char *) NULL, kCanvasOptScroll},
    {TK_OPTION_STRING, "-takefocus", "takeFocus", "TakeFocus",
     (char *) "", Tk_Offset(CanvasWidget, takeFocusObj),
     -1, TK_OPTION_NULL_OK, (char *) NULL, kCanvasOptOther},
    {TK_OPTION_PIXELS, "-width", "width", "Width",
     (char *) "400", -1, Tk_Offset(CanvasWidget, width),
     0, (char *) NULL, kCanvasOptGeometry},
    {TK_OPTION_PIXELS, "-xscrollincrement", "xScrollIncrement",
     "XScrollIncrement", (char *) "0", -1,
     Tk_Offset(CanvasWidget, xScrollIncrement), 0, (char *) NULL, kCanvasOptScroll},
    {TK_OPTION_STRING, "-xscrollcommand", "xScrollCommand", "XScrollCommand",
     (char *) "", Tk_Offset(CanvasWidget, xScrollCommandObj),
     -1, 0, (char *) NULL, kCanvasOptScroll},
    {TK_OPTION_PIXELS, "-yscrollincrement", "yScrollIncrement",
     "YScrollIncrement", (char *) "0", -1,
     Tk_Offset(CanvasWidget, yScrollIncrement), 0, (char *) NULL, kCanvasOptScroll},
    {TK_OPTION_STRING, "-yscrollcommand", "yScrollCommand", "YScrollCommand",
     (char *) "", Tk_Offset(CanvasWidget, yScrollCommandObj),
     -1, 0, (char *) NULL, kCanvasOptScroll},
    {TK_OPTION_END, NULL, NULL, NULL, NULL, 0, 0, 0, NULL, 0}
};

int ParseScrollRegion(Tcl_Interp* interp, Tk_Window tkwin,
                      const char* text, double values[4]) {
    if (!interp || !text || !*text) return TCL_ERROR;

    int argc = 0;
    const char** argv = nullptr;
    if (Tcl_SplitList(interp, text, &argc, &argv) != TCL_OK) {
        Tcl_Free(reinterpret_cast<char*>(argv ? argv : nullptr));
        return TCL_ERROR;
    }
    if (argc != 4) {
        Tcl_Free(reinterpret_cast<char*>(argv));
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("bad scrollRegion \"%s\"", text));
        Tcl_SetErrorCode(interp, "TK", "CANVAS", "SCROLL_REGION", nullptr);
        return TCL_ERROR;
    }

    int rc = TCL_OK;
    for (int i = 0; i < 4; ++i) {
        int pixels = 0;
        // Tk_GetPixels sets the interp error on bad input
        // ("bad screen distance ..."); Tk replaces it with "bad scrollRegion".
        if (Tk_GetPixels(interp, tkwin, argv[i], &pixels) != TCL_OK) {
            rc = TCL_ERROR;
            break;
        }
        values[i] = static_cast<double>(pixels);
    }
    Tcl_Free(reinterpret_cast<char*>(argv));
    if (rc != TCL_OK) {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("bad scrollRegion \"%s\"", text));
        Tcl_SetErrorCode(interp, "TK", "CANVAS", "SCROLL_REGION", nullptr);
    }
    return rc;
}

}  // namespace

Tk_OptionTable CreateCanvasOptionTable(Tcl_Interp* interp) {
    return Tk_CreateOptionTable(interp, kCanvasOptionSpecs);
}

bool ApplyCanvasOptions(CanvasWidget& widget, int mask) {
    if (!widget.tkwin) return true;
    bool ok = true;
    std::string scrollRegionError;

    if (mask & kCanvasOptColour) {
        const char* spec = widget.backgroundObj ? Tcl_GetString(widget.backgroundObj) : "white";
        Color color;
        if (ResolveTkColor(widget.interp, widget.tkwin, spec, color)) {
            widget.backgroundColor = color;
            widget.surface.SetBackground(color);
        }
    }

    if (mask & kCanvasOptGeometry) {
        // Ask Tk for the requested size; the geometry manager may override it.
        Tk_GeometryRequest(widget.tkwin, widget.width, widget.height);
    }

    if (mask & kCanvasOptScroll) {
        widget.model.View().SetConfine(widget.confine != 0);
        widget.model.View().SetXScrollIncrement(widget.xScrollIncrement);
        widget.model.View().SetYScrollIncrement(widget.yScrollIncrement);

        double rect[4];
        const char* region = widget.scrollRegionObj
            ? Tcl_GetString(widget.scrollRegionObj) : "";
        if (!*region) {
            // Empty string means "no scroll region" (Tk's TK_CONFIG_NULL_OK).
            widget.model.View().SetScrollRegion(BBox::Empty());
        } else if (ParseScrollRegion(widget.interp, widget.tkwin, region, rect) == TCL_OK) {
            widget.model.View().SetScrollRegion(
                BBox{rect[0], rect[1], rect[2], rect[3]});
        } else {
            widget.model.View().SetScrollRegion(BBox::Empty());
            scrollRegionError = std::string("bad scrollRegion \"") + region + "\"";
            ok = false;
        }
        const CanvasView& view = widget.model.View();
        widget.model.View().SetOrigin(view.XOffset(), view.YOffset());
    }

    if (mask & kCanvasOptOther) {
        if (widget.classObj) {
            const char* className = Tcl_GetString(widget.classObj);
            if (className && *className) Tk_SetClass(widget.tkwin, className);
        }
    }

    widget.UpdateGeometry();
    widget.Invalidate();
    if (!ok) {
        Tcl_SetObjResult(widget.interp, Tcl_NewStringObj(scrollRegionError.c_str(), -1));
        Tcl_SetErrorCode(widget.interp, "TK", "CANVAS", "SCROLL_REGION", nullptr);
    }
    return ok;
}

int CanvasReportOptions(Tcl_Interp* interp, CanvasWidget& widget) {
    Tcl_Obj* dict = Tcl_NewDictObj();

    for (const Tk_OptionSpec* spec = kCanvasOptionSpecs; spec->type != TK_OPTION_END; ++spec) {
        // Skip synonyms: they would duplicate the canonical option.
        if (spec->type == TK_OPTION_SYNONYM) continue;

        Tcl_Obj* nameObj = Tcl_NewStringObj(spec->optionName, -1);
        Tcl_Obj* value = Tk_GetOptionValue(interp, reinterpret_cast<char*>(&widget),
                                            widget.optionTable, nameObj, widget.tkwin);
        if (!value) {
            Tcl_ResetResult(interp);
            continue;
        }
        Tcl_DictObjPut(interp, dict, nameObj, value);
    }

    Tcl_SetObjResult(interp, dict);
    return TCL_OK;
}

}  // namespace tkd2dcanvas