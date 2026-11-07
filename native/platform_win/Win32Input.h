// Win32Input.h — translates Win32 mouse/keyboard messages into canvas input
// events. The glue layer (CanvasEventBridge) subscribes to these.
#pragma once

#include <windows.h>
#include <optional>
#include <cstdint>

enum class CanvasInputKind {
    ButtonPress,
    ButtonRelease,
    Motion,
    Enter,
    Leave,
    KeyPress,
    KeyRelease,
    FocusIn,
    FocusOut,
    MouseWheel,
};

struct CanvasInputEvent {
    CanvasInputKind kind = CanvasInputKind::Motion;
    double canvasX = 0.0;   // canvas units (1:1 with pixels pre-DPI-scaling)
    double canvasY = 0.0;
    int button = 0;         // 1=left, 2=middle, 3=right (Tk numbering)
    int keyCode = 0;        // VK_* for KeyPress/KeyRelease
    int wheelDelta = 0;     // WHEEL_DELTA units
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
};

namespace Win32Input {

// Maps a Win32 message to a canvas input event, or nullopt if not relevant.
std::optional<CanvasInputEvent> FromMessage(UINT msg, WPARAM wParam, LPARAM lParam);

// Current modifier keys as a mask of MK_SHIFT/MK_CONTROL plus alt flag.
bool IsShiftDown();
bool IsCtrlDown();
bool IsAltDown();

}  // namespace Win32Input
