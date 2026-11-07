// CanvasEventBridge.h — bridges Tk window events into the canvas widget.
//
// The canvas keeps all of its event logic in the C++ core (per the design
// in docs/architecture.md); this layer only maintains the state that Tk
// owns: geometry, mapping and destruction.
#pragma once

#include <tcl.h>
#include <tk.h>

namespace tkd2dcanvas {

struct CanvasWidget;

// Installs the Tk event handlers on the widget window.
void InstallEventHandlers(CanvasWidget& widget);

}  // namespace tkd2dcanvas
