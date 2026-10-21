#pragma once
#include <windows.h>

namespace cvr::input {
// The plugin and game IATs report virtual rendering dimensions. Mouse messages
// and absolute SendInput coordinates still arrive in real window pixels.
inline BOOL PhysicalClientRect(HWND window, LPRECT rect) {
    using Function=BOOL(WINAPI*)(HWND,LPRECT);
    static const auto original=reinterpret_cast<Function>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetClientRect"));
    return original ? original(window,rect) : FALSE;
}
} // namespace cvr::input
