// Win32Dpi.h — DPI helpers (per-monitor awareness is handled by Tk/manifest).
#pragma once

#include <windows.h>

namespace Win32Dpi {
    // DPI for the given window; falls back to 96 when unavailable.
    UINT DpiOfWindow(HWND hwnd);

    // Scale factor (1.0 = 96 DPI).
    double ScaleOfWindow(HWND hwnd);

    // Scale a canvas-unit length to physical pixels.
    int Scale(double value, double scale);

    // True when the process runs per-monitor DPI aware v2 (or better).
    bool IsPerMonitorAware();
}
