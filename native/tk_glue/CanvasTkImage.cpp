#include "CanvasTkImage.h"

namespace tkd2dcanvas {

bool TkPhotoToRgba(Tcl_Interp* interp, Tk_Window /*tkwin*/,
                   const std::string& imageName, RgbaImage& /*out*/)
{
    // M9 roadmap item. Tk_FindPhoto + Tk_PhotoGetImage give us a
    // Tk_PhotoImageBlock with per-offset channel pointers and row pitch;
    // converting pitch/offsets into tightly packed RGBA is straightforward.
    // The risky part is memory ownership of pixelPtr across Tk versions,
    // so the conversion is intentionally left unimplemented until verified
    // against the exact Tk 8.6 build we link against.
    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
        "image \"%s\": photo-to-RGBA conversion not implemented yet (milestone M9)",
        imageName.c_str()));
    return false;
}

}  // namespace tkd2dcanvas
