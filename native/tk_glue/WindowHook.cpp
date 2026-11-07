#include "WindowHook.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <vector>

#include "CanvasWidget.h"

namespace tkd2dcanvas {

namespace {

struct WindowHook {
    WNDPROC original = nullptr;
    std::vector<CanvasWidget*> canvases;
};

std::map<HWND, WindowHook>& Hooks() {
    static std::map<HWND, WindowHook> hooks;
    return hooks;
}

void Restore(HWND hwnd, WindowHook& hook) {
    if (hook.original) {
        SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(hook.original));
        hook.original = nullptr;
    }
    Hooks().erase(hwnd);
}

LRESULT CALLBACK HookedWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto& hooks = Hooks();
    auto it = hooks.find(hwnd);
    if (it == hooks.end()) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    WindowHook& hook = it->second;

    switch (msg) {
        case WM_PAINT: {
            // The damaged area (in window coordinates) is captured before Tk
            // validates the update region.
            RECT damaged{};
            const bool haveDamaged = GetUpdateRect(hwnd, &damaged, FALSE) != 0;

            // Let Tk paint the widget's own background first.
            LRESULT result = CallWindowProcW(hook.original, hwnd, msg, wParam, lParam);

            if (haveDamaged) {
                HDC dc = GetDC(hwnd);
                if (dc) {
                    for (CanvasWidget* canvas : hook.canvases) {
                        if (canvas) canvas->Paint(dc, damaged);
                    }
                    ReleaseDC(hwnd, dc);
                }
            }
            return result;
        }
        case WM_ERASEBKGND:
            // Let Tk erase the widget background as usual; the canvas content
            // is drawn afterwards in WM_PAINT.
            return CallWindowProcW(hook.original, hwnd, msg, wParam, lParam);
        case WM_NCDESTROY:
            Restore(hwnd, hook);
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        default:
            return CallWindowProcW(hook.original, hwnd, msg, wParam, lParam);
    }
}

}  // namespace

void AttachCanvasToWindow(HWND hwnd, CanvasWidget* canvas) {
    if (!hwnd || !canvas) return;

    auto& hooks = Hooks();
    auto it = hooks.find(hwnd);
    if (it == hooks.end()) {
        WindowHook hook;
        hook.original = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(HookedWndProc)));
        if (!hook.original) return;  // subclassing failed
        it = hooks.emplace(hwnd, hook).first;
    }

    auto& list = it->second.canvases;
    if (std::find(list.begin(), list.end(), canvas) == list.end()) {
        list.push_back(canvas);
    }
}

void DetachCanvasFromWindow(HWND hwnd, CanvasWidget* canvas) {
    if (!hwnd) return;

    auto& hooks = Hooks();
    auto it = hooks.find(hwnd);
    if (it == hooks.end()) return;

    auto& list = it->second.canvases;
    list.erase(std::remove(list.begin(), list.end(), canvas), list.end());

    if (list.empty()) {
        Restore(hwnd, it->second);
    }
}

}  // namespace tkd2dcanvas
