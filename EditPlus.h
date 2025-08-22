#pragma once
#include <windows.h>
#include <windowsx.h>
#include <CommCtrl.h>
#include <crtdbg.h>

// TODO Use Edit_GetFile* instead of actual line

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

inline bool IsStartOfNewLine(LPCTSTR lpText, int index)
{
    return index == 0 || lpText[index - 1] == TEXT('\r') || lpText[index - 1] == TEXT('\n');
}

inline void Edit_ReplaceLineEndings(HWND hWnd)
{
    const LPCTSTR sEol[4] = { TEXT(""), TEXT("\r\n"), TEXT("\n"), TEXT("\r") };
    const EC_ENDOFLINE eol = Edit_GetEndOfLine(hWnd);
    const int len = lstrlen(sEol[eol]);

    DWORD nSelStart, nSelEnd;
    Edit_GetSelEx(hWnd, &nSelStart, &nSelEnd);

    SetWindowRedraw(hWnd, FALSE);
    const HCURSOR hCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));

    const int count = Edit_GetLineCount(hWnd);
    for (int i = 0; i < count - 1; ++i)
    {
        _ASSERT(count == Edit_GetLineCount(hWnd));
        HANDLE hText = Edit_GetHandle(hWnd);
        _ASSERT(hText);
        LPCTSTR lpText = (LPCTSTR) LocalLock(hText);
        _ASSERT(lpText);

        const DWORD nextindex = Edit_LineIndex(hWnd, i + 1);
        if (IsStartOfNewLine(lpText, nextindex))
        {
            LocalUnlock(hText);

            const DWORD begin = nextindex - (lpText[nextindex - 2] == TEXT('\r') ? 2 : 1);
            if ((nextindex - begin) != len || memcmp(lpText + begin, sEol[eol], (nextindex - begin) * sizeof(TCHAR)) != 0)
            {
                Edit_SetSel(hWnd, begin, nextindex);
                Edit_ReplaceSelEx(hWnd, sEol[eol], TRUE);
                if (begin < nSelStart)
                    nSelStart += len - (int)(nextindex - begin);
                if (begin < nSelEnd)
                    nSelEnd += len - (int) (nextindex - begin);
            }
        }
        else
            LocalUnlock(hText);
    }

    Edit_SetSel(hWnd, nSelStart, nSelEnd);
    SetCursor(hCursor);
    SetWindowRedraw(hWnd, TRUE);
}

inline int EditGetActualLine(HWND hWnd, int line, LPCTSTR lpText)
{
    int actualline = 0;
    for (int i = 0; i < line; ++i)
    {
        const int index = Edit_LineIndex(hWnd, i);
        if (IsStartOfNewLine(lpText, index))
            ++actualline;
    }
    return actualline;
}

inline POINT EditGetPos(HWND hWnd, const DWORD dwCaret)
{
    const bool bWrap = (GetWindowStyle(hWnd) & WS_HSCROLL) == 0;
    if (bWrap)
    {
        const HLOCAL hText = Edit_GetHandle(hWnd);
        _ASSERT(hText);
        LPCTSTR lpText = (LPCTSTR) LocalLock(hText);
        _ASSERT(lpText);

        const int nLine = Edit_LineFromChar(hWnd, dwCaret);

        int nLineBegin = nLine;
        while (!IsStartOfNewLine(lpText, Edit_LineIndex(hWnd, nLineBegin)))
            --nLineBegin;

        const int nActualLine = EditGetActualLine(hWnd, nLineBegin, lpText);
        const int nLineIndex = Edit_LineIndex(hWnd, nLineBegin);

        LocalUnlock(hText);

        return { (LONG) (dwCaret - nLineIndex + 1), (LONG) (nActualLine + 1) };
    }
    else
    {
        const int nLine = Edit_LineFromChar(hWnd, dwCaret);
        const int nLineIndex = Edit_LineIndex(hWnd, nLine);

        return { (LONG) (dwCaret - nLineIndex + 1), (LONG) (nLine + 1) };
    }
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

// Edit_GetCaretIndex 
inline DWORD EditEx_GetCaret(HWND hwndCtl) { return ((DWORD) SNDMSG((hwndCtl), EM_EX_GETCARET, 0, 0)); }
inline void EditEx_SetStyle(HWND hwndCtl, DWORD dwExStyle) { ((void) SNDMSG((hwndCtl), EM_EX_SETSTYLE, (WPARAM) (dwExStyle), 0)); }
inline DWORD EditEx_GetStyle(HWND hwndCtl) { return ((DWORD) SNDMSG((hwndCtl), EM_EX_GETSTYLE, 0, 0)); }
