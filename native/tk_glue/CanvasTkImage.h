// CanvasTkImage.h — resolve Tk photo images into raw RGBA pixels for Direct2D.
// Tk 8.6 provides Tk_FindPhoto / Tk_PhotoGetImage; bitmaps (-bitmap) are
// handled separately via XBM parsing (not implemented yet).
#pragma once

#include <tcl.h>
#include <tk.h>

#include <cstdint>
#include <string>
#include <vector>

namespace tkd2dcanvas {

struct RgbaImage {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;  // RGBA, row-major, tightly packed
};

// Copies the current pixels of the named Tk photo image.
// Returns false (and sets interp error) when the image is missing or
// the requested operation is unsupported (see M9 notes in docs/roadmap.md).
bool TkPhotoToRgba(Tcl_Interp* interp, Tk_Window tkwin, const std::string& imageName, RgbaImage& out);

}  // namespace tkd2dcanvas
