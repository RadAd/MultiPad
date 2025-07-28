#pragma once
#include <windows.h>
#include <CommDlg.h>
#include "GdiPlus.h"
#include "Rad/WinError.h"

#define Edit_GetSelEx(hwndCtl, ichStart, ichEnd)  ((void)SNDMSG((hwndCtl), EM_GETSEL, (WPARAM)(ichStart), (LPARAM)(ichEnd)))

inline void NotifyParent(HWND hWnd, int code)
{
    HWND hWndParent = GetParent(hWnd);
    SendMessage(GetParent(hWnd), WM_COMMAND, MAKEWPARAM(GetWindowID(hWnd), code), (LPARAM) hWnd);
    //FORWARD_WM_COMMAND(GetParent(hWnd), GetWindowID(hWnd), hWnd, code, SNDMSG);
}

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

struct EditData
{
    HLOCAL hText;
    DWORD nUnknown1;
    DWORD nLimitText;
    BYTE b[5];
    DWORD nSelStart;
    DWORD nSelEnd;
    DWORD nCursor;
    BYTE c[156];
    HFONT hFont; // 192
};

const EditData* EditGetData(HWND hEdit)
{
    return reinterpret_cast<EditData*>(GetWindowLongPtr(hEdit, 0));
}

inline DWORD EditGetCaret(HWND hEdit)
{
#if 0
    // NOTE There is no message to get the cursor position
    // This works by unselecting the reselecting
    SetWindowRedraw(hEdit, FALSE);
    DWORD nSelStart, nSelEnd;
    Edit_GetSelEx(hEdit, &nSelStart, &nSelEnd);
    Edit_SetSel(hEdit, (DWORD) -1, (DWORD) -1);
    DWORD nCursor;
    EditGetSel(hEdit, &nCursor, nullptr);
    if (nCursor == nSelStart)
        Edit_SetSel(hEdit, nSelEnd, nSelStart);
    else
        Edit_SetSel(hEdit, nSelStart, nSelEnd);
    SetWindowRedraw(hEdit, TRUE);
    return nCursor;
#else
    const EditData* edp = EditGetData(hEdit);
    return edp->nCursor;
#endif
}

inline POINT EditGetPos(HWND hWnd, const DWORD dwCursor)
{
    const DWORD dwLine = Edit_LineFromChar(hWnd, dwCursor);
    const DWORD dwLineIndex = Edit_LineIndex(hWnd, dwLine);
    return { (LONG) (dwCursor - dwLineIndex + 1), (LONG) (dwLine + 1) };
}

#define EN_SEL_CHANGED 0x0A00

struct EditExData
{
    DWORD nPos; // Last mouse position
    bool viewWhiteSpace = false; // Show whitespace
};

inline void ModifyWhiteSpace(LPTSTR lpText, const SIZE_T size, bool add)
{
    if (add)
    {
        for (int i = 0; i < size; ++i)
        {
            TCHAR& c = lpText[i];
            if (c == TEXT(' '))
                c = 0x2423;
            else if (c == TEXT('\t'))
                c = 0x2409;
        }
    }
    else
    {
        for (int i = 0; i < size; ++i)
        {
            TCHAR& c = lpText[i];
            if (c == 0x2423)
                c = TEXT(' ');
            else if (c == 0x2409)
                c = TEXT('\t');
        }
    }
}

inline void EditExSetViewWhiteSpace(HWND hWnd, bool viewWhiteSpace)
{
    HANDLE hText = Edit_GetHandle(hWnd);
    const SIZE_T size = GlobalSize(hText) / sizeof(TCHAR);
    LPTSTR lpText = (LPTSTR) GlobalLock(hText);
    if (lpText)
    {
        ModifyWhiteSpace(lpText, size, viewWhiteSpace);
        GlobalUnlock(hText);
        InvalidateRect(hWnd, NULL, TRUE);
    }
}

#define EM_EX_GETVIEWWHITESPACE 0x00F0
#define EM_EX_SETVIEWWHITESPACE 0x00F1

inline LRESULT CALLBACK EditExProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR /*uIdSubclass*/, DWORD_PTR dwRefData)
{
    EditExData* eexd = reinterpret_cast<EditExData*>(dwRefData);

    LRESULT ret = 0;
    
    switch (uMsg)
    {
    case WM_CHAR:
        if (eexd->viewWhiteSpace)
            ModifyWhiteSpace(reinterpret_cast<LPTSTR>(&wParam), 1, true);
        ret = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        break;
    case EM_EX_GETVIEWWHITESPACE:
        ret = eexd->viewWhiteSpace ? TRUE : FALSE;
        break;
    case EM_EX_SETVIEWWHITESPACE:
    {
        const bool viewWhiteSpace = wParam;
        if (eexd->viewWhiteSpace != viewWhiteSpace)
        {
            EditExSetViewWhiteSpace(hWnd, viewWhiteSpace);
            eexd->viewWhiteSpace = viewWhiteSpace;
        }
        break;
    }
    case WM_COPY:
    {
        const EditData* edp = EditGetData(hWnd);
        if (edp->nSelStart != edp->nSelEnd)
        {
            const DWORD nLength = edp->nSelEnd - edp->nSelStart;
            const HANDLE hClip = GlobalAlloc(GMEM_MOVEABLE, (nLength + 1) * sizeof(TCHAR));
            LPTSTR lpClip = (LPTSTR) GlobalLock(hClip);
            if (lpClip)
            {
                const HANDLE hText = Edit_GetHandle(hWnd);
                LPCTSTR lpText = (LPTSTR) GlobalLock(hText);
                if (lpText)
                {
                    CopyMemory(lpClip, lpText + edp->nSelStart, nLength * sizeof(TCHAR));
                    lpClip[nLength] = TEXT('\0');
                    GlobalUnlock(hText);

                    if (eexd->viewWhiteSpace)
                        ModifyWhiteSpace(lpClip, nLength, false);
                }
                GlobalUnlock(hClip);

                while (!OpenClipboard(hWnd))
                    ;
                EmptyClipboard();
#ifdef _UNICODE
                SetClipboardData(CF_UNICODETEXT, hClip);
#else
                SetClipboardData(CF_TEXT, hClip);
#endif
                CloseClipboard();
            }
        }
        break;
    }
    case WM_PASTE:
    {
        while (!OpenClipboard(hWnd))
            ;
        const HANDLE hClip = GetClipboardData(CF_UNICODETEXT);
        LPTSTR lpClip = (LPTSTR) GlobalLock(hClip);
        if (lpClip)
        {
            const SIZE_T size = GlobalSize(hClip) / sizeof(TCHAR);
            //Edit_ReplaceSel(hWnd, lpClip);
            ((void) SNDMSG((hWnd), EM_REPLACESEL, TRUE, (LPARAM) (LPCTSTR) (lpClip)));
            GlobalUnlock(hClip);
        }
        CloseClipboard();
        break;
    }
    case EM_REPLACESEL:
    {
        const EditData* edp = EditGetData(hWnd);
        const DWORD dwSelStart = edp->nSelStart;
        ret = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        if (eexd->viewWhiteSpace)
        {
            const HANDLE hText = Edit_GetHandle(hWnd);
            LPTSTR lpText = (LPTSTR) GlobalLock(hText);
            if (lpText)
            {
                ModifyWhiteSpace(lpText + dwSelStart, edp->nCursor - dwSelStart, true);
                GlobalUnlock(hText);
            }
            InvalidateRect(hWnd, NULL, TRUE);
        }
        NotifyParent(hWnd, EN_SEL_CHANGED);
        break;
    }
    case WM_SETTEXT:
        ret = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        if (eexd->viewWhiteSpace)
            EditExSetViewWhiteSpace(hWnd, true);
        break;
    case WM_GETTEXT:
        ret = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        if (eexd->viewWhiteSpace)
            ModifyWhiteSpace(reinterpret_cast<LPTSTR>(lParam), wParam, false);
        break;

    case EM_SETSEL:
    case EM_UNDO:
    //case EM_REPLACESEL:
    //case WM_PASTE:
    case WM_KEYDOWN:
    case WM_LBUTTONDOWN:
        ret = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        NotifyParent(hWnd, EN_SEL_CHANGED);
        break;
    case WM_MOUSEMOVE:
        ret = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        if (wParam & MK_LBUTTON && eexd->nPos != (DWORD) lParam)
            NotifyParent(hWnd, EN_SEL_CHANGED);
        eexd->nPos = (DWORD) lParam;
        break;
    default:
        ret = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        break;
    }

    if (uMsg == WM_DESTROY)
        delete eexd;

    return ret;
}
