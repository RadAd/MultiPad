#pragma once
#include <windows.h>

inline LONG Width(const RECT r)
{
    return r.right - r.left;
}

inline LONG Height(const RECT r)
{
    return r.bottom - r.top;
}

inline BOOL ScreenToClient(HWND hWnd, LPRECT lpRect)
{
    LPPOINT pt = (LPPOINT) lpRect;
    return ScreenToClient(hWnd, &pt[0]) && ScreenToClient(hWnd, &pt[1]);
}
