// CanvasItemOptions.h — parsing and application of canvas *item* options
// (-fill / -outline / -width / -dash / -tags). Widget-level options live in
// CanvasOptions.*; item options attach to C++ items created through
// ItemFactory. No direct D2D/Win32 dependencies.
#pragma once

#include <tcl.h>
#include <tk.h>

class CanvasItem;  // global namespace (tkd2dcanvas/CanvasItem.h)

namespace tkd2dcanvas {

struct CanvasWidget;

// Applies one "-option value" pair to `item`. `itemId` is the model id (used
// for tag operations). Returns false with the interpreter error set when the
// option is unknown or the value is invalid.
bool ApplyItemOption(Tcl_Interp* interp, CanvasWidget& widget, int itemId,
                     CanvasItem& item, const char* option, Tcl_Obj* valueObj);

}  // namespace tkd2dcanvas
