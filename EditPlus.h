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

inline void Edit_ReplaceLineEndings(HWND hWnd)
{
    const LPCTSTR sEol[4] = { TEXT(""), TEXT("\r\n"), TEXT("\n"), TEXT("\r") };
    const EC_ENDOFLINE eol = Edit_GetEndOfLine(hWnd);
    const int len = lstrlen(sEol[eol]);

    DWORD nSelStart, nSelEnd;
    Edit_GetSelEx(hWnd, &nSelStart, &nSelEnd);

    SetWindowRedraw(hWnd, FALSE);
    const HCURSOR hCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));

    const DWORD count = Edit_GetFileLineCount(hWnd);
    for (DWORD i = 0; i < count - 1; ++i)
    {
        _ASSERT(count == Edit_GetFileLineCount(hWnd));
        HANDLE hText = Edit_GetHandle(hWnd);
        _ASSERT(hText);
        LPCTSTR lpText = (LPCTSTR) LocalLock(hText);
        _ASSERT(lpText);

        const DWORD index = Edit_GetFileLineIndex(hWnd, i);
        const DWORD length = Edit_GetFileLineLength(hWnd, index);
        const DWORD nextindex = Edit_GetFileLineIndex(hWnd, i + 1);

        const DWORD begin = index + length;
        if ((nextindex - begin) != len || memcmp(lpText + begin, sEol[eol], (nextindex - begin) * sizeof(TCHAR)) != 0)
        {
            LocalUnlock(hText);

            Edit_SetSel(hWnd, begin, nextindex);
            // TODO What if we send a VK_RETURN in a WM_CHAR?
            Edit_ReplaceSelEx(hWnd, sEol[eol], TRUE);
            if (begin < nSelStart)
                nSelStart += len - (int) (nextindex - begin);
            if (begin < nSelEnd)
                nSelEnd += len - (int) (nextindex - begin);
        }
        else
            LocalUnlock(hText);
    }

    Edit_SetSel(hWnd, nSelStart, nSelEnd);
    SetCursor(hCursor);
    SetWindowRedraw(hWnd, TRUE);
}

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

#define EM_EX_GETCARET      0x00F0
//#define EM_EX_SETCARET    0x00F1
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

// TODO Replace EditEx_GetCaret with Edit_GetCaretIndex 
inline DWORD EditEx_GetCaret(HWND hwndCtl) { return ((DWORD) SNDMSG((hwndCtl), EM_EX_GETCARET, 0, 0)); }
inline void EditEx_SetStyle(HWND hwndCtl, DWORD dwExStyle) { ((void) SNDMSG((hwndCtl), EM_EX_SETSTYLE, (WPARAM) (dwExStyle), 0)); }
inline DWORD EditEx_GetStyle(HWND hwndCtl) { return ((DWORD) SNDMSG((hwndCtl), EM_EX_GETSTYLE, 0, 0)); }
