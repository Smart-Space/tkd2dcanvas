// CanvasOptions.h — widget-level configuration (-width, -height, -background,
// -scrollregion, ...). Item-level options live in CanvasItemCommand.
#pragma once

#include <tcl.h>

#include <string>
#include <vector>
#include <optional>

struct CanvasOptions {
    int width = 400;
    int height = 300;
    std::string background = "white";
    std::string cursor;                 // empty = Tk default
    int borderWidth = 0;
    int highlightThickness = 0;
    int xScrollIncrement = 0;
    int yScrollIncrement = 0;
    double closeEnough = 2.0;
    bool confine = true;
    bool takeFocus = false;
    std::string scrollRegion;           // "" = not configured
    std::string xScrollCommand;         // "" = none
    std::string yScrollCommand;         // "" = none
    std::vector<std::pair<std::string, std::string>> unknown;  // preserved, warned later
};

namespace tkd2dcanvas {

// Parses option/value pairs following the widget path argument.
// On failure returns false and sets interp error message.
bool ParseWidgetOptions(Tcl_Interp* interp,
                        int objc,
                        Tcl_Obj* const objv[],
                        CanvasOptions& out);

}  // namespace tkd2dcanvas
