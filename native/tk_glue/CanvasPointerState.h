// CanvasPointerState.h — shared pointer-state helpers for the tag bindings.
//
// X11 exposes the pressed buttons as modifier bits inside every event's
// state field; Tk uses the same bits for its canvas pick logic. Defined
// here so both the event bridge and the pick implementation agree.
#pragma once

#include <tk.h>

namespace tkd2dcanvas {

// TkGetButtonMask (tkEvent.c) applied to Tk's X11 button constants.
inline unsigned ButtonMaskOf(unsigned button) {
    switch (button) {
        case Button1: return Button1Mask;
        case Button2: return Button2Mask;
        case Button3: return Button3Mask;
        case Button4: return Button4Mask;
        case Button5: return Button5Mask;
        default: return 0;
    }
}

// Tk's ALL_BUTTONS (tkInt.h).
inline constexpr unsigned kAllButtonMasks = Button1Mask | Button2Mask | Button3Mask | Button4Mask | Button5Mask;

}  // namespace tkd2dcanvas
