// Glue-layer font resolution + measurement entry points.
#pragma once

#include <tcl.h>
#include <tk.h>

#include <string>
#include <vector>

#include "CanvasTkFont.h"

namespace tkd2dcanvas {

struct MeasuredTextLayout {
    std::vector<std::string> lines;
    std::vector<double> lineWidths;
    double maxLineWidth = 0.0;
};

// Splits `text` into lines no wider than `wrapWidth` canvas units
// (`wrapWidth <= 0` or `wrapMode == "none"` disables wrapping) using
// `wrapMode` ("word" | "char") for the granularity. Measurements use Tk's
// own GDI path (`font measure` == Tk_TextWidth, integer pixels), so the
// wrap decisions match what Tk's window canvas would do.
// Newlines ("\n" and "\r\n") always force a line break. The result always
// holds at least one line (possibly empty).
MeasuredTextLayout MeasureTextLayout(Tcl_Interp* interp, Tk_Window tkwin,
                                     const TkFontInfo& font, const std::string& text,
                                     double wrapWidth, const std::string& wrapMode);

}  // namespace tkd2dcanvas
