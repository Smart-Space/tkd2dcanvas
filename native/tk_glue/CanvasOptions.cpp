#include "CanvasOptions.h"

#include <cstdlib>
#include <string>
#include <vector>

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
     (char *) "2.0", -1, Tk_Offset(CanvasWidget, closeEnough),
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
    {TK_OPTION_RELIEF, "-relief", "relief", "Relief",
     (char *) "flat", -1, Tk_Offset(CanvasWidget, relief),
     0, (char *) NULL, kCanvasOptOther},
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

bool ParseRectList(const char* text, double values[4]) {
    if (!text || !*text) return false;

    std::vector<double> numbers;
    const char* p = text;
    while (*p) {
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == '\0') break;
        char* end = nullptr;
        const double v = std::strtod(p, &end);
        if (end == p) return false;
        numbers.push_back(v);
        p = end;
    }
    if (numbers.size() != 4) return false;
    for (int i = 0; i < 4; ++i) values[i] = numbers[i];
    return true;
}

}  // namespace

Tk_OptionTable CreateCanvasOptionTable(Tcl_Interp* interp) {
    return Tk_CreateOptionTable(interp, kCanvasOptionSpecs);
}

void ApplyCanvasOptions(CanvasWidget& widget, int mask) {    if (!widget.tkwin) return;

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
        if (ParseRectList(region, rect)) {
            widget.model.View().SetScrollRegion(
                BBox{rect[0], rect[1], rect[2], rect[3]});
        } else {
            widget.model.View().SetScrollRegion(BBox::Empty());
        }
    }

    if (mask & kCanvasOptOther) {
        if (widget.classObj) {
            const char* className = Tcl_GetString(widget.classObj);
            if (className && *className) Tk_SetClass(widget.tkwin, className);
        }
    }

    widget.UpdateGeometry();
    widget.Invalidate();
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