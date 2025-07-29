#pragma once
#include <windows.h>
#include <windowsx.h>
#include <CommCtrl.h>
#include "GdiPlus.h"
#include "Rad/WinError.h"

//#define Edit_GetSelEx(hwndCtl, ichStart, ichEnd)  ((void)SNDMSG((hwndCtl), EM_GETSEL, (WPARAM)(ichStart), (LPARAM)(ichEnd)))
inline void Edit_GetSelEx(HWND hwndCtl, DWORD* ichStart, DWORD* ichEnd) { ((void) SNDMSG((hwndCtl), EM_GETSEL, (WPARAM) (ichStart), (LPARAM) (ichEnd)));  }
//#define Edit_ReplaceSelEx(hwndCtl, lpszReplace, allowundo)   ((void)SNDMSG((hwndCtl), EM_REPLACESEL, allowundo, (LPARAM)(LPCTSTR)(lpszReplace)))
inline void Edit_ReplaceSelEx(HWND hwndCtl, LPCTSTR lpszReplace, BOOL allowundo) { ((void) SNDMSG((hwndCtl), EM_REPLACESEL, allowundo, (LPARAM) (LPCTSTR) (lpszReplace))); }
//#define Edit_Scroll(hwndCtl, dv, dh)            ((void)SNDMSG((hwndCtl), EM_LINESCROLL, (WPARAM)(dh), (LPARAM)(dv)))
inline void Edit_ScrollEx(HWND hwndCtl, UINT action) { ((void) SNDMSG((hwndCtl), EM_SCROLL, (WPARAM) (action), 0)); }

inline HWND Edit_Create(HWND hParent, DWORD dwStyle, RECT rc, int id)
{
    HWND hWnd = CreateWindow(
        WC_EDIT,
        TEXT(""),
        dwStyle,
        rc.left, rc.top, Width(rc), Height(rc),
        hParent,
        (HMENU) (INT_PTR) id,
        NULL,
        NULL);
    CHECK_LE(hWnd != NULL);
    return hWnd;
}

inline POINT EditGetPos(HWND hWnd, const DWORD dwCaret)
{
    const DWORD dwLine = Edit_LineFromChar(hWnd, dwCaret);
    const DWORD dwLineIndex = Edit_LineIndex(hWnd, dwLine);
    return { (LONG) (dwCaret - dwLineIndex + 1), (LONG) (dwLine + 1) };
}

#define EN_SEL_CHANGED 0x0A00

#define EM_EX_GETCARET          0x00F0
//#define EM_EX_SETCARET          0x00F1
#define EM_EX_GETVIEWWHITESPACE 0x00F2
#define EM_EX_SETVIEWWHITESPACE 0x00F3

void InitEditEx(HWND hWnd);

inline void EditEx_GetCaret(HWND hWnd, DWORD* nCaret)
{
    SendMessage(hWnd, EM_EX_GETCARET, (WPARAM) nCaret, 0);
}

inline void EditEx_SetViewWhiteSpace(HWND hWnd, BOOL viewWhiteSpace)
{
    SendMessage(hWnd, EM_EX_SETVIEWWHITESPACE, (WPARAM) viewWhiteSpace, 0);
}

inline BOOL EditEx_GetViewWhiteSpace(HWND hWnd)
{
    return (BOOL) SendMessage(hWnd, EM_EX_GETVIEWWHITESPACE, 0, 0);
}
