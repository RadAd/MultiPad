#pragma once
#include <windows.h>
#include <windowsx.h>
#include <CommCtrl.h>
#include <crtdbg.h>

inline HWND Edit_Create(HWND hParent, DWORD dwStyle, RECT rc, int id)
{
    HWND hWnd = CreateWindow(
        WC_EDIT,
        TEXT(""),
        dwStyle,
        rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
        hParent,
        (HMENU) (INT_PTR) id,
        NULL,
        NULL);
    _ASSERT(hWnd != NULL);
    return hWnd;
}

inline POINT EditGetPos(HWND hWnd, const DWORD dwCaret)
{
    const DWORD dwLine = Edit_LineFromChar(hWnd, dwCaret);
    const DWORD dwLineIndex = Edit_LineIndex(hWnd, dwLine);
    return { (LONG) (dwCaret - dwLineIndex + 1), (LONG) (dwLine + 1) };
}

void InitEditEx(HWND hWnd);

//#define Edit_GetSelEx(hwndCtl, ichStart, ichEnd)  ((void)SNDMSG((hwndCtl), EM_GETSEL, (WPARAM)(ichStart), (LPARAM)(ichEnd)))
//#define Edit_ReplaceSelEx(hwndCtl, lpszReplace, allowundo)   ((void)SNDMSG((hwndCtl), EM_REPLACESEL, allowundo, (LPARAM)(LPCTSTR)(lpszReplace)))
//#define Edit_Scroll(hwndCtl, dv, dh)            ((void)SNDMSG((hwndCtl), EM_LINESCROLL, (WPARAM)(dh), (LPARAM)(dv)))

inline void Edit_GetSelEx(HWND hwndCtl, DWORD* ichStart, DWORD* ichEnd) { ((void) SNDMSG((hwndCtl), EM_GETSEL, (WPARAM) (ichStart), (LPARAM) (ichEnd))); }
inline void Edit_ReplaceSelEx(HWND hwndCtl, LPCTSTR lpszReplace, BOOL allowundo) { ((void) SNDMSG((hwndCtl), EM_REPLACESEL, allowundo, (LPARAM) (LPCTSTR) (lpszReplace))); }
inline void Edit_ScrollEx(HWND hwndCtl, UINT action) { ((void) SNDMSG((hwndCtl), EM_SCROLL, (WPARAM) (action), 0)); }

// Edit Plus Messages

#define EM_EX_GETCARET      0x00F0
//#define EM_EX_SETCARET    0x00F1
#define EM_EX_GETSTYLE      0x00F2
#define EM_EX_SETSTYLE      0x00F3

// Edit Plus Notifications

#define EN_SEL_CHANGED 0x0A00

// Edit Plus Styles

#define ES_EX_VIEWWHITESPACE 0x00000001

// whitespace
// line numbers
// bookmarks
// show unprintable characters
// tabs/spaces


inline void EditEx_GetCaret(HWND hwndCtl, DWORD* dwCaret) { ((void) SNDMSG((hwndCtl), EM_EX_GETCARET, (WPARAM) (dwCaret), 0)); }
inline void EditEx_SetStyle(HWND hwndCtl, DWORD dwExStyle) { ((void) SNDMSG((hwndCtl), EM_EX_SETSTYLE, (WPARAM) (dwExStyle), 0)); }
inline DWORD EditEx_GetStyle(HWND hwndCtl) { ((DWORD) SNDMSG((hwndCtl), EM_EX_GETSTYLE, 0, 0)); }
