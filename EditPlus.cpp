#include "EditPlus.h"

#ifndef _DEBUG
#define _VERIFY(x) (x)
#else
#define _VERIFY(x) _ASSERT(x)
#endif

namespace
{
    inline LONG Width(const RECT r)
    {
        return r.right - r.left;
    }

    inline LONG Height(const RECT r)
    {
        return r.bottom - r.top;
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

    inline const EditData* EditGetData(HWND hEdit)
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

    inline void NotifyParent(HWND hWnd, int code)
    {
        const HWND hWndParent = GetParent(hWnd);
        FORWARD_WM_COMMAND(GetParent(hWnd), GetWindowID(hWnd), hWnd, code, SNDMSG);
    }

    inline void ModifyWhiteSpace(LPTSTR lpText, const SIZE_T size, bool add)
    {
        //const TCHAR vspace = 0x2423;  // White space indicator
        const TCHAR vspace = 0x00B7; // Middle dot
        //const TCHAR vtab = 0x21E5; // Rightwards arrow to bar
        //const TCHAR vtab = 0x27FC; // Rightwards arrow to bar with horizontal stroke
        const TCHAR vtab = 0x27F6; // Rightwards arrow to bar with vertical stroke
        if (add)
        {
            for (int i = 0; i < size; ++i)
            {
                TCHAR& c = lpText[i];
                if (c == TEXT(' '))
                    c = vspace;
                else if (c == TEXT('\t'))
                    c = vtab;
            }
        }
        else
        {
            for (int i = 0; i < size; ++i)
            {
                TCHAR& c = lpText[i];
                if (c == vspace)
                    c = TEXT(' ');
                else if (c == vtab)
                    c = TEXT('\t');
            }
        }
    }

    inline void EditExSetViewWhiteSpace(HWND hWnd, bool viewWhiteSpace)
    {
        const HANDLE hText = Edit_GetHandle(hWnd);
        const SIZE_T size = GlobalSize(hText) / sizeof(TCHAR);
        _ASSERT(size != 0);
        LPTSTR lpText = (LPTSTR) GlobalLock(hText);
        _ASSERT(lpText);
        if (lpText)
        {
            ModifyWhiteSpace(lpText, size, viewWhiteSpace);
            GlobalUnlock(hText);
            _VERIFY(InvalidateRect(hWnd, NULL, TRUE));
        }
    }

    inline SIZE GetLineNumberSize(HWND hWnd, HDC hDC)
    {
        SIZE sz = {};
        {
            SIZE szText = {};
            GetTextExtentPoint32(hDC, TEXT("99999"), 5, &szText);
            sz.cx += szText.cx;
            if (szText.cy > sz.cy)
                sz.cy = szText.cy;
        }
        return sz;
    }
}

struct EditExData
{
    DWORD nPos; // Last mouse position
    DWORD nLineNumberWidth = 0;
    DWORD dwExStyle = ES_EX_USETABS;
    DWORD nTabSize = 32 / 4;
};

LRESULT EditExProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR dwRefData)
{
    EditExData* eexd = reinterpret_cast<EditExData*>(dwRefData);

    LRESULT ret = 0;

    switch (uMsg)
    {
    case EM_EX_GETCARET:
        ret = EditGetCaret(hWnd);
        break;
    case EM_EX_GETSTYLE:
        ret = eexd->dwExStyle;
        break;
    case EM_EX_SETSTYLE:
    {
        DWORD dwExStyle = (DWORD) wParam;
        if ((eexd->dwExStyle & ES_EX_VIEWWHITESPACE) != (dwExStyle & ES_EX_VIEWWHITESPACE))
            EditExSetViewWhiteSpace(hWnd, wParam & ES_EX_VIEWWHITESPACE);
        if ((eexd->dwExStyle & ES_EX_LINENUMBERS) != (dwExStyle & ES_EX_LINENUMBERS))
        {
            if (dwExStyle & ES_EX_LINENUMBERS)
            {
                const HDC hDC = GetDC(hWnd);
                SelectFont(hDC, GetWindowFont(hWnd));
                const SIZE sz = GetLineNumberSize(hWnd, hDC);
                ReleaseDC(hWnd, hDC);

                eexd->nLineNumberWidth = sz.cx;

                RECT rc;
                Edit_GetRect(hWnd, &rc);
                rc.left += eexd->nLineNumberWidth;
                Edit_SetRect(hWnd, &rc);
            }
            else
            {
                RECT rc;
                Edit_GetRect(hWnd, &rc);
                rc.left -= eexd->nLineNumberWidth;
                Edit_SetRect(hWnd, &rc);

                eexd->nLineNumberWidth = 0;
            }
        }
        eexd->dwExStyle = dwExStyle;
        break;
    }
    case WM_CHAR:
        if (wParam == VK_TAB && (eexd->dwExStyle & ES_EX_USETABS) == 0)
        {
            // TODO Implement shift+Tab
            DWORD nSelStart, nSelEnd;
            Edit_GetSelEx(hWnd, &nSelStart, &nSelEnd);
            const POINT pos = EditGetPos(hWnd, nSelStart);
            const int x = eexd->nTabSize - (pos.x - 1) % eexd->nTabSize;
            TCHAR s[10] = TEXT("         ");
            _ASSERT(x < ARRAYSIZE(s));
            s[x] = TEXT('\0');
            Edit_ReplaceSel(hWnd, s);
            ret = 0;
        }
        else
        {
            if (eexd->dwExStyle & ES_EX_VIEWWHITESPACE)
                ModifyWhiteSpace(reinterpret_cast<LPTSTR>(&wParam), 1, true);
            ret = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        }
        break;
    case WM_KEYDOWN:
        if (wParam == VK_UP && (GetKeyState(VK_CONTROL) & 0x8000))
            Edit_ScrollEx(hWnd, SB_LINEUP);
        else if (wParam == VK_DOWN && (GetKeyState(VK_CONTROL) & 0x8000))
            Edit_ScrollEx(hWnd, SB_LINEDOWN);
        else if (wParam == VK_PRIOR && (GetKeyState(VK_CONTROL) & 0x8000))
            Edit_ScrollEx(hWnd, SB_PAGEUP);
        else if (wParam == VK_NEXT && (GetKeyState(VK_CONTROL) & 0x8000))
            Edit_ScrollEx(hWnd, SB_PAGEDOWN);
        else
        {
            ret = DefSubclassProc(hWnd, uMsg, wParam, lParam);
            if (IsWindowVisible(hWnd))
                NotifyParent(hWnd, EN_SEL_CHANGED);
        }
        break;
    case WM_CUT:
        ret = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        // TODO Unsure why but WM_CUT not longer deletes the selection when WM_COPY is overridden
        Edit_ReplaceSelEx(hWnd, TEXT(""), TRUE);
        break;
    case WM_COPY:
    {
        DWORD nSelStart, nSelEnd;
        Edit_GetSelEx(hWnd, &nSelStart, &nSelEnd);
        if (nSelStart != nSelEnd)
        {
            const DWORD nLength = nSelEnd - nSelStart;
            const HANDLE hClip = GlobalAlloc(GMEM_MOVEABLE, (nLength + 1) * sizeof(TCHAR));
            _ASSERT(hClip);
            LPTSTR lpClip = (LPTSTR) GlobalLock(hClip);
            _ASSERT(lpClip);
            if (lpClip)
            {
                const HANDLE hText = Edit_GetHandle(hWnd);
                LPCTSTR lpText = (LPTSTR) GlobalLock(hText);
                _ASSERT(lpText);
                if (lpText)
                {
                    CopyMemory(lpClip, lpText + nSelStart, nLength * sizeof(TCHAR));
                    lpClip[nLength] = TEXT('\0');
                    GlobalUnlock(hText);

                    if (eexd->dwExStyle & ES_EX_VIEWWHITESPACE)
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
        if (hClip)
        {
            LPTSTR lpClip = (LPTSTR) GlobalLock(hClip);
            _ASSERT(lpClip);
            if (lpClip)
            {
                const SIZE_T size = GlobalSize(hClip) / sizeof(TCHAR);
                _ASSERT(size != 0);
                Edit_ReplaceSelEx(hWnd, lpClip, TRUE);
                GlobalUnlock(hClip);
            }
        }
        _VERIFY(CloseClipboard());
        break;
    }
    case EM_REPLACESEL:
    {
        DWORD nSelStart, nSelEnd;
        Edit_GetSelEx(hWnd, &nSelStart, &nSelEnd);
        const DWORD dwSelStart = nSelStart;
        ret = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        if (eexd->dwExStyle & ES_EX_VIEWWHITESPACE)
        {
            const HANDLE hText = Edit_GetHandle(hWnd);
            LPTSTR lpText = (LPTSTR) GlobalLock(hText);
            _ASSERT(lpText);
            if (lpText)
            {
                const DWORD nCaretPos = EditGetCaret(hWnd);
                ModifyWhiteSpace(lpText + dwSelStart, nCaretPos - dwSelStart, true);
                GlobalUnlock(hText);
            }
            _VERIFY(InvalidateRect(hWnd, NULL, TRUE));
        }
        if (IsWindowVisible(hWnd))
            NotifyParent(hWnd, EN_SEL_CHANGED);
        break;
    }
    case EM_SETTABSTOPS:
        if (wParam == 0)
            eexd->nTabSize = 32 / 4;
        else
            eexd->nTabSize = *((const int*) lParam) / 4;
        ret = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        break;
    case WM_SETTEXT:
        ret = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        if (eexd->dwExStyle & ES_EX_VIEWWHITESPACE)
            EditExSetViewWhiteSpace(hWnd, true);
        break;
    case WM_GETTEXT:
        ret = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        if (eexd->dwExStyle & ES_EX_VIEWWHITESPACE)
            ModifyWhiteSpace(reinterpret_cast<LPTSTR>(lParam), wParam, false);
        break;

    case EM_SETSEL:
    case EM_UNDO:
    //case EM_REPLACESEL:
    //case WM_PASTE:
    //case WM_KEYDOWN:
    case WM_LBUTTONDOWN:
        ret = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        if (IsWindowVisible(hWnd))
            NotifyParent(hWnd, EN_SEL_CHANGED);
        break;
    case WM_MOUSEMOVE:
        ret = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        if (wParam & MK_LBUTTON && eexd->nPos != (DWORD) lParam && IsWindowVisible(hWnd))
            NotifyParent(hWnd, EN_SEL_CHANGED);
        eexd->nPos = (DWORD) lParam;
        break;

    case WM_PAINT:
    {
        ret = DefSubclassProc(hWnd, uMsg, wParam, lParam);

        const HDC hDC = GetDC(hWnd);
        SelectFont(hDC, GetWindowFont(hWnd));

        RECT rc = {};
        GetClientRect(hWnd, &rc);
        rc.right = rc.left + eexd->nLineNumberWidth;

        const COLORREF color = GetSysColor(COLOR_MENU);
        // TODO Use WM_CTLCOLORGUTTER
        SetBkColor(hDC, color);
        SetTextColor(hDC, GetSysColor(COLOR_MENUTEXT));
        SetDCPenColor(hDC, color);
        SetDCBrushColor(hDC, color);
        SelectPen(hDC, GetStockObject(DC_PEN));
        SelectBrush(hDC, GetStockObject(DC_BRUSH));

        Rectangle(hDC, rc.left, rc.top, rc.right, rc.bottom);

        const bool bWrap = (GetWindowStyle(hWnd) & WS_HSCROLL) == 0;

        const HLOCAL hText = Edit_GetHandle(hWnd);
        _ASSERT(hText);
        LPCTSTR lpText = (LPCTSTR) LocalLock(hText);
        _ASSERT(lpText);

        const int count = Edit_GetLineCount(hWnd);
        const int first = Edit_GetFirstVisibleLine(hWnd);

        RECT rcText = rc;
        int linenum = bWrap ? EditGetActualLine(hWnd, first, lpText) : first;
        for (int line = first; line < count; ++line)
        {
            const int index = Edit_LineIndex(hWnd, line);
            if (!bWrap || IsStartOfNewLine(lpText, index))
            {
                ++linenum;

                const POINT ptPos = Edit_GetPosFromChar(hWnd, index);
                rcText.top = ptPos.y;
                if (rcText.top > rc.bottom)
                    break;

                TCHAR text[100];
                int len = wsprintf(text, TEXT("%d"), linenum);
                UINT clip = DT_NOCLIP;
                if (rcText.bottom > rc.bottom)
                {
                    rcText.bottom = rc.bottom;
                    clip = 0;
                }
                DrawText(hDC, text, len, &rcText, DT_RIGHT | DT_TOP | clip);
            }
        }
        LocalUnlock(hText);
        ReleaseDC(hWnd, hDC);
        break;
    }
    case WM_SIZE:
    {
        const SIZE sz = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ret = DefSubclassProc(hWnd, uMsg, wParam, lParam);

        RECT rc;
        Edit_GetRect(hWnd, &rc);
        rc.left += eexd->nLineNumberWidth;
        Edit_SetRect(hWnd, &rc);
        break;
    }

    default:
        ret = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        break;
    }

    if (uMsg == WM_DESTROY)
        delete eexd;

    return ret;
}

void InitEditEx(HWND hWnd)
{
    _ASSERT(GetWindowStyle(hWnd) & ES_MULTILINE);
    _VERIFY(SetWindowSubclass(hWnd, EditExProc, 0, reinterpret_cast<DWORD_PTR>(new EditExData({}))));
}
