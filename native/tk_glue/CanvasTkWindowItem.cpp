// CanvasTkWindowItem.cpp — glue for Tk-embedded child widgets (-window option).
// Milestone M10; the most complex item kind. Kept as a stub so the build
// links cleanly; behavior is provided later.
#include <tcl.h>
#include <tk.h>

namespace tkd2dcanvas {

// TODO(M10): resolve -window path via Tk_NameToWindow, Tk_MakeWindowExist,
// track Map/Unmap/Configure, clip child HWND to canvas item bbox, route
// events when the canvas widget holds focus, and handle -width/-height
// sizing options. See docs/tk_canvas_compat_matrix.md.

}  // namespace tkd2dcanvas
