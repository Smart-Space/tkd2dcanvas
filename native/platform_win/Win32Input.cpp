#include "Win32Input.h"

#include <windowsx.h>  // GET_X_LPARAM / GET_Y_LPARAM / GET_WHEEL_DELTA_WPARAM / GET_KEYSTATE_WPARAM

namespace Win32Input {

std::optional<CanvasInputEvent> FromMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    CanvasInputEvent ev{};

    // Modifier flags come from low-order word of wParam only for mouse messages;
    // keyboard messages carry the virtual key in wParam instead.
    const bool mouseMessage =
        (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) ||
        msg == WM_MOUSEWHEEL || msg == WM_MOUSELEAVE;
    if (mouseMessage) {
        const WORD keys = GET_KEYSTATE_WPARAM(wParam);
        ev.shift = (keys & MK_SHIFT) != 0;
        ev.ctrl = (keys & MK_CONTROL) != 0;
    } else {
        ev.shift = IsShiftDown();
        ev.ctrl = IsCtrlDown();
    }
    ev.alt = IsAltDown();

    const auto xy = [&]() {
        ev.canvasX = static_cast<double>(GET_X_LPARAM(lParam));
        ev.canvasY = static_cast<double>(GET_Y_LPARAM(lParam));
    };

    switch (msg) {
        case WM_LBUTTONDOWN: xy(); ev.kind = CanvasInputKind::ButtonPress; ev.button = 1; return ev;
        case WM_MBUTTONDOWN: xy(); ev.kind = CanvasInputKind::ButtonPress; ev.button = 2; return ev;
        case WM_RBUTTONDOWN: xy(); ev.kind = CanvasInputKind::ButtonPress; ev.button = 3; return ev;
        case WM_LBUTTONUP:   xy(); ev.kind = CanvasInputKind::ButtonRelease; ev.button = 1; return ev;
        case WM_MBUTTONUP:   xy(); ev.kind = CanvasInputKind::ButtonRelease; ev.button = 2; return ev;
        case WM_RBUTTONUP:   xy(); ev.kind = CanvasInputKind::ButtonRelease; ev.button = 3; return ev;
        case WM_MOUSEMOVE:   xy(); ev.kind = CanvasInputKind::Motion; return ev;
        case WM_MOUSELEAVE:  ev.kind = CanvasInputKind::Leave; return ev;
        case WM_MOUSEWHEEL:
            ev.kind = CanvasInputKind::MouseWheel;
            ev.wheelDelta = static_cast<int>(GET_WHEEL_DELTA_WPARAM(wParam));
            // NOTE: lParam for WM_MOUSEWHEEL is in screen coordinates; convert
            // with ScreenToClient at the call site (needs the HWND).
            ev.canvasX = static_cast<double>(GET_X_LPARAM(lParam));
            ev.canvasY = static_cast<double>(GET_Y_LPARAM(lParam));
            return ev;
        case WM_KEYDOWN: ev.kind = CanvasInputKind::KeyPress;  ev.keyCode = static_cast<int>(wParam); return ev;
        case WM_KEYUP:   ev.kind = CanvasInputKind::KeyRelease; ev.keyCode = static_cast<int>(wParam); return ev;
        case WM_SETFOCUS: ev.kind = CanvasInputKind::FocusIn;  return ev;
        case WM_KILLFOCUS: ev.kind = CanvasInputKind::FocusOut; return ev;
        default: return std::nullopt;
    }
}

bool IsShiftDown() { return (GetKeyState(VK_SHIFT) & 0x8000) != 0; }
bool IsCtrlDown()  { return (GetKeyState(VK_CONTROL) & 0x8000) != 0; }
bool IsAltDown()   { return (GetKeyState(VK_MENU) & 0x8000) != 0; }

}  // namespace Win32Input
