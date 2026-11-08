// CanvasTkFont.h — resolve Tk font specs ("TkDefaultFont", {Helvetica 12 bold})
// into DirectWrite-usable font information.
#pragma once

#include <tcl.h>
#include <tk.h>

#include <string>

namespace tkd2dcanvas {

struct TkFontInfo {
    std::string family = "TkDefaultFont";
    double size = 12.0;      // PIXELS: Tk point sizes converted with `tk scaling` (points * pixels/point)
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool overstrike = false;
};

struct TkFontMetrics {
    double ascent = 0.0;
    double descent = 0.0;
    double lineSpace() const { return ascent + descent; }
};

// Uses the Tcl command `font actual -displayof <window> -font <spec>`,
// which is stable public API (unlike Tk's private font internals) and
// yields the fully resolved dictionary for the current display.
// On failure returns false and sets interp error message.
bool ResolveTkFont(Tcl_Interp* interp, Tk_Window tkwin, const char* spec, TkFontInfo& out);

// Measure an UTF-8 string with the exact same Tk path the canvas uses
// for text (`font measure <spec> <text>` == Tk_TextWidth == GDI extent,
// integer pixels). Returns false when the call fails; `width` keeps its
// previous value then.
bool MeasureTkFont(Tcl_Interp* interp, Tk_Window tkwin, const TkFontInfo& font,
                   const std::string& utf8, int& width);

// Queries `font metrics <spec>` for the exact pixel-size face matching
// `font` — Tk reports the metrics of the font it would draw for the
// given size, so a negative (pixel) size yields the metrics for exactly
// the size the renderer uses. This is the same ascent/descent pair
// Tk_ComputeTextLayout uses as the line step for wrapped text.
// On failure returns false and sets interp's error message.
bool QueryTkFontMetrics(Tcl_Interp* interp, Tk_Window tkwin, const TkFontInfo& font, TkFontMetrics& out);

}  // namespace tkd2dcanvas
