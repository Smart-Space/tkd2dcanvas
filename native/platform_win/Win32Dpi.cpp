#include "Win32Dpi.h"

namespace Win32Dpi {

UINT DpiOfWindow(HWND hwnd) {
    if (!hwnd) return 96;
    using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        auto pGetDpiForWindow = reinterpret_cast<GetDpiForWindowFn>(
            GetProcAddress(user32, "GetDpiForWindow"));
        if (pGetDpiForWindow) return pGetDpiForWindow(hwnd);
    }
    return 96;
}

double ScaleOfWindow(HWND hwnd) {
    return static_cast<double>(DpiOfWindow(hwnd)) / 96.0;
}

int Scale(double value, double scale) {
    return static_cast<int>(value * scale + 0.5);
}

bool IsPerMonitorAware() {
    // DPI_AWARENESS_PER_MONITOR_AWARE_V2 == -4 in the Windows SDK.
    const HANDLE pm = reinterpret_cast<HANDLE>(static_cast<INT_PTR>(-4));
    if (GetThreadDpiAwarenessContext == nullptr) return false;  // XP-era SDK fallback
    return GetThreadDpiAwarenessContext() == pm;
}

}  // namespace Win32Dpi
