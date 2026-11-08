// CanvasTkImage.h — resolve Tk photo images into raw RGBA pixels for Direct2D.
// Tk 8.6 provides Tk_FindPhoto / Tk_PhotoGetImage; bitmaps (-bitmap) are
// handled via XBM parsing (Tk builtin names and @file.xbm).
#pragma once

#include <tcl.h>
#include <tk.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "tkd2dcanvas/Color.h"

namespace tkd2dcanvas {

struct RgbaImage {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;  // RGBA, row-major, tightly packed, straight alpha
};

// Copies the current pixels of the named Tk photo image (straight alpha).
// Returns false and sets interp's error message when the image does not
// exist or is not a photo image.
bool TkPhotoToRgba(Tcl_Interp* interp, Tk_Window tkwin, const std::string& imageName, RgbaImage& out);

struct XbmBitmap {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> bits;  // row-major, (width + 7) / 8 bytes per row, LSB first
};

// Resolves a Tk bitmap name to its 1-bit source:
//   "@file.xbm"     — parse the XBM file at that path (Tk semantics),
//   "name"          — one of Tk's predefined bitmaps (error, info, ...),
//   anything else   — an error is set on interp.
// Returns false on any failure.
bool TkBitmapToBits(Tcl_Interp* interp, const std::string& bitmapName, XbmBitmap& out);

// Expands a 1-bit bitmap to RGBA (straight alpha). Set bits use `foreground`
// (when null they stay transparent); clear bits use `background` (when null
// they stay transparent).
RgbaImage XbmToRgba(const XbmBitmap& bitmap,
                    const std::optional<Color>& foreground,
                    const std::optional<Color>& background);

}  // namespace tkd2dcanvas
