// Win32ChildWindow.h — optional raw-child-HWND helper. Tk normally owns the
// widget window; this is used only for experiments (e.g. the custom painting
// surface inside a subclassed Tk window).
#pragma once

#include <windows.h>
#include <string>

class Win32ChildWindow {
public:
    ~Win32ChildWindow();

    bool Create(HWND parent, const wchar_t* className, int x, int y, int width, int height);
    void Destroy();

    HWND Handle() const { return hwnd_; }
    bool Valid() const { return hwnd_ != nullptr; }

private:
    HWND hwnd_ = nullptr;
    bool registeredClass_ = false;
    static const wchar_t* ClassName();
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
};
