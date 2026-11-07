// Tkd2dcanvasInit.cpp — Tcl package entry point.
//
// Build product: tkd2dcanvas01.dll, loaded via `package require tkd2dcanvas`.
// Exports Tkd2dcanvas_Init (and a SafeInit stub that refuses safe interpreters).
#include <tcl.h>
#include <tk.h>

#include <windows.h>

#include "D2DContext.h"
#include "CanvasCmd.h"

namespace {
constexpr const char* kPackageVersion = "0.1";
}

extern "C" __declspec(dllexport) int
Tkd2dcanvas_Init(Tcl_Interp* interp)
{
    if (Tcl_InitStubs(interp, "8.6", 0) == nullptr) {
        return TCL_ERROR;
    }
    if (Tk_InitStubs(interp, "8.6", 0) == nullptr) {
        return TCL_ERROR;
    }

    // Direct2D / DirectWrite factory creation is lightweight; it fails only
    // on systems without usable D3D, even in software fallback.
    if (!D2DContext::EnsureInitialized()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("direct2d failed to initialize", -1));
        return TCL_ERROR;
    }

    if (tkd2dcanvas::RegisterCommands(interp) != TCL_OK) {
        return TCL_ERROR;
    }

    Tcl_PkgProvide(interp, "tkd2dcanvas", kPackageVersion);
    return TCL_OK;
}

extern "C" __declspec(dllexport) int
Tkd2dcanvas_SafeInit(Tcl_Interp* interp)
{
    // Not safe for untrusted interpreters: widget creates a real HWND and uses Direct2D.
    Tcl_SetObjResult(interp, Tcl_NewStringObj(
        "tkd2dcanvas is not safe for safe interpreters", -1));
    return TCL_ERROR;
}
