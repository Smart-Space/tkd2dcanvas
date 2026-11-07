#include "Win32ChildWindow.h"

namespace {
const wchar_t kClassName[] = L"Tkd2dcanvasChild";
}

Win32ChildWindow::~Win32ChildWindow() { Destroy(); }

const wchar_t* Win32ChildWindow::ClassName() { return kClassName; }

bool Win32ChildWindow::Create(HWND parent, const wchar_t* /*className*/,
                              int x, int y, int width, int height) {
    if (!parent) return false;

    WNDCLASSW wc{};
    wc.lpfnWndProc = &Win32ChildWindow::WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = ClassName();
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));  // IDC_ARROW
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    if (!GetClassInfoW(wc.hInstance, ClassName(), &wc)) {
        if (!RegisterClassW(&wc)) return false;
        registeredClass_ = true;
    }

    hwnd_ = CreateWindowExW(0, ClassName(), L"tkd2dcanvas",
                            WS_CHILD | WS_VISIBLE,
                            x, y, width, height,
                            parent, nullptr, wc.hInstance, this);
    return hwnd_ != nullptr;
}

void Win32ChildWindow::Destroy() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

LRESULT CALLBACK Win32ChildWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;  // background handled in WM_PAINT via double buffering
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd, &ps);
            // Default: clear to COLOR_WINDOW until a surface is attached.
            HBRUSH brush = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
            FillRect(hdc, &ps.rcPaint, brush);
            EndPaint(hwnd, &ps);
            return 0;
        }
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}
