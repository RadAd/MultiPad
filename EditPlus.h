#pragma once
#include <windows.h>
#include <windowsx.h>
#include <CommCtrl.h>
#include <crtdbg.h>

//#define MAKEPOINT(l)       (*((LPPOINT)&(l)))
inline POINT MAKEPOINT(DWORD l)
{
    POINT pt;
    pt.x = LOWORD(l);
    pt.y = HIWORD(l);
    return pt;
}

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

//#define Edit_GetSelEx(hwndCtl, ichStart, ichEnd)  ((void)SNDMSG((hwndCtl), EM_GETSEL, (WPARAM)(ichStart), (LPARAM)(ichEnd)))
//#define Edit_ReplaceSelEx(hwndCtl, lpszReplace, allowundo)   ((void)SNDMSG((hwndCtl), EM_REPLACESEL, allowundo, (LPARAM)(LPCTSTR)(lpszReplace)))
//#define Edit_Scroll(hwndCtl, dv, dh)            ((DWORD)SNDMSG((hwndCtl), EM_LINESCROLL, (WPARAM)(dh), (LPARAM)(dv)))

inline void Edit_GetSelEx(HWND hwndCtl, DWORD* ichStart, DWORD* ichEnd) { ((void) SNDMSG((hwndCtl), EM_GETSEL, (WPARAM) (ichStart), (LPARAM) (ichEnd))); }
inline void Edit_ReplaceSelEx(HWND hwndCtl, LPCTSTR lpszReplace, BOOL allowundo) { ((void) SNDMSG((hwndCtl), EM_REPLACESEL, allowundo, (LPARAM) (LPCTSTR) (lpszReplace))); }
inline DWORD Edit_ScrollEx(HWND hwndCtl, UINT action) { return ((DWORD) SNDMSG((hwndCtl), EM_SCROLL, (WPARAM) (action), 0)); }
inline POINT Edit_GetPosFromChar(HWND hwndCtl, UINT nChar) { return MAKEPOINT((DWORD) SNDMSG((hwndCtl), EM_POSFROMCHAR, (WPARAM) (nChar), 0)); }
inline int Edit_GetLimitText(HWND hwndCtl) { return ((int) SNDMSG((hwndCtl), EM_GETLIMITTEXT, 0, 0)); }

void Edit_ReplaceLineEndings(HWND hWnd);

inline POINT EditGetPos(HWND hWnd, const DWORD dwCaret)
{
    const int nLine = Edit_GetFileLineFromChar(hWnd, dwCaret);
    const int nLineIndex = Edit_GetFileLineIndex(hWnd, nLine);

    return { (LONG) (dwCaret - nLineIndex + 1), (LONG) (nLine + 1) };
}

inline DWORD EditGetFirstVisibleFileLine(HWND hWnd)
{
    const int first = Edit_GetFirstVisibleLine(hWnd);
    const int index = Edit_LineIndex(hWnd, first);
    return Edit_GetFileLineFromChar(hWnd, index);
}

void InitEditEx(HWND hWnd);

// Edit Plus Messages

#define EM_EX_GETSTYLE      0x00F2
#define EM_EX_SETSTYLE      0x00F3

// Edit Plus Notifications

#define EN_SEL_CHANGED 0x0A00

// Edit Plus Styles

#define ES_EX_VIEWWHITESPACE    0x00000001
#define ES_EX_LINENUMBERS       0x00000002
#define ES_EX_USETABS           0x00000004

// TODO
// bookmarks
// show unprintable characters

// Edit_SetCaretIndex has an error newCaretPosition/newCaretIndex
inline BOOL Edit_SetCaretIndexEx(HWND hwndCtl, int newCaretPosition) { return (BOOL) SNDMSG((hwndCtl), EM_SETCARETINDEX, (WPARAM) (newCaretPosition), 0); }

inline void EditEx_SetStyle(HWND hwndCtl, DWORD dwExStyle) { ((void) SNDMSG((hwndCtl), EM_EX_SETSTYLE, (WPARAM) (dwExStyle), 0)); }
inline DWORD EditEx_GetStyle(HWND hwndCtl) { return ((DWORD) SNDMSG((hwndCtl), EM_EX_GETSTYLE, 0, 0)); }
