// CanvasEventBridge.h — bridges Win32/Tk input into CanvasModel events.
// Decision (see docs/architecture.md): item bindings (<Button-1> on a tag)
// are resolved by the C++ core, not by replaying synthetic events into Tk.
#pragma once

#include <tcl.h>
#include <tk.h>

namespace tkd2dcanvas {

struct CanvasWidget;

// Installs Tk event handlers on the widget window. Called by CanvasWidget::Create.
void InstallEventHandlers(CanvasWidget& widget);

// Removes handlers; safe to call when never installed.
void RemoveEventHandlers(CanvasWidget& widget);

}  // namespace tkd2dcanvas
