// WindowHook.h — hooks a widget's Win32 window so that the canvas can paint
// itself with Direct2D.
//
// On Windows every Tk widget owns a child window of class "TkChild"; Tk draws
// the widget's background into it and delivers WM_PAINT to it.  We subclass
// that window: Tk paints first (the original window procedure), then we
// render the canvas content into the same DC with Direct2D.
//
// Because the canvas has its own HWND we keep Tk's event routing, stacking
// order and clipping for free - only the pixels are ours.
#pragma once

#include <windows.h>

namespace tkd2dcanvas {

struct CanvasWidget;

// Installs (or refreshes) the window procedure hook for `hwnd` and registers
// `canvas` to be painted by it.
void AttachCanvasToWindow(HWND hwnd, CanvasWidget* canvas);

// Removes `canvas` from the hook. The window procedure is restored once the
// last canvas of that HWND is gone.
void DetachCanvasFromWindow(HWND hwnd, CanvasWidget* canvas);

}  // namespace tkd2dcanvas
