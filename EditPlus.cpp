#include "EditPlus.h"
#include <strsafe.h>

#ifndef _DEBUG
#define _VERIFY(x) (x)
#else
#define _VERIFY(x) _ASSERT(x)
#endif

namespace
{
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
    DWORD nTrailingWhitespaceWidth = 2;
    DWORD nTabSize = 32 / 4;
};

LRESULT EditExProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR dwRefData)
{
    EditExData* eexd = reinterpret_cast<EditExData*>(dwRefData);

    LRESULT ret = 0;

    switch (uMsg)
    {
    case EM_SETEXTENDEDSTYLE:
    {
        const DWORD dwOldExStyle = Edit_GetExtendedStyle(hWnd);
        const DWORD dwExStyle = (DWORD) wParam & (DWORD) lParam;
        if ((dwOldExStyle & ES_EX_VIEWWHITESPACE) != (dwExStyle & ES_EX_VIEWWHITESPACE))
            EditExSetViewWhiteSpace(hWnd, wParam & ES_EX_VIEWWHITESPACE);
        if ((dwOldExStyle & ES_EX_LINENUMBERS) != (dwExStyle & ES_EX_LINENUMBERS))
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
        ret = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        break;
    }
    case EM_SETCARETINDEX:
        ret = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        if (IsWindowVisible(hWnd))
            NotifyParent(hWnd, EN_SEL_CHANGED);
        break;
    case WM_CHAR:
    {
        const DWORD dwExStyle = Edit_GetExtendedStyle(hWnd);
        if (wParam == VK_TAB && (dwExStyle & ES_EX_USETABS) == 0)
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
            if (dwExStyle & ES_EX_VIEWWHITESPACE)
                ModifyWhiteSpace(reinterpret_cast<LPTSTR>(&wParam), 1, true);
            ret = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        }
        {
            // TODO Could be more efficient - only invalidate the line where the caret is and only if editing the end of the line
            RECT rc = {};
            GetClientRect(hWnd, &rc);
            rc.left = rc.right - eexd->nTrailingWhitespaceWidth;
            _VERIFY(InvalidateRect(hWnd, &rc, TRUE));
        }
        break;
    }
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

                    if (Edit_GetExtendedStyle(hWnd) & ES_EX_VIEWWHITESPACE)
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
        if (Edit_GetExtendedStyle(hWnd) & ES_EX_VIEWWHITESPACE)
        {
            const HANDLE hText = Edit_GetHandle(hWnd);
            LPTSTR lpText = (LPTSTR) GlobalLock(hText);
            _ASSERT(lpText);
            if (lpText)
            {
                const DWORD nCaretPos = Edit_GetCaretIndex(hWnd);
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
        if (Edit_GetExtendedStyle(hWnd) & ES_EX_VIEWWHITESPACE)
            EditExSetViewWhiteSpace(hWnd, true);
        break;
    case WM_GETTEXT:
        ret = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        if (Edit_GetExtendedStyle(hWnd) & ES_EX_VIEWWHITESPACE)
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

        if (eexd->nLineNumberWidth < 0 && eexd->nTrailingWhitespaceWidth < 0)
            break;

        RECT rcClient = {};
        _VERIFY(GetClientRect(hWnd, &rcClient));

        const DWORD count = Edit_GetFileLineCount(hWnd);
        const DWORD first = EditGetFirstVisibleFileLine(hWnd);

        const HDC hDC = GetDC(hWnd);

        if (eexd->nLineNumberWidth > 0)
        {
            SelectFont(hDC, GetWindowFont(hWnd));

            RECT rc = rcClient;
            rc.right = rc.left + eexd->nLineNumberWidth;

            const COLORREF color = GetSysColor(COLOR_MENU);
            const HBRUSH hBrush = GetStockBrush(DC_BRUSH);
            // TODO Use WM_CTLCOLORGUTTER
            SetBkColor(hDC, color);
            SetTextColor(hDC, GetSysColor(COLOR_MENUTEXT));
            SelectBrush(hDC, hBrush);
            SetDCBrushColor(hDC, color);

            FillRect(hDC, &rc, GetStockBrush(DC_BRUSH));

            RECT rcText = rc;
            for (DWORD line = first; line < count; ++line)
            {
                const DWORD index = Edit_GetFileLineIndex(hWnd, line);
                const POINT ptPos = Edit_GetPosFromChar(hWnd, index);
                rcText.top = ptPos.y;
                if (rcText.top >= rc.bottom)
                    break;

                const DWORD indexnext = Edit_GetFileLineIndex(hWnd, line + 1);
                const POINT ptPosNext = Edit_GetPosFromChar(hWnd, indexnext);
                // TODO How to find bottom of last line?
                rcText.bottom = ptPosNext.y;

                TCHAR text[100];
                LPTSTR pEnd = nullptr;
                _VERIFY(SUCCEEDED(StringCchPrintfEx(text, ARRAYSIZE(text), &pEnd, nullptr, 0, TEXT("%d"), line + 1)));
                const int len = static_cast<int>(pEnd - text);

                UINT clip = DT_NOCLIP;
                if (rcText.bottom > rc.bottom)
                {
                    rcText.bottom = rc.bottom;
                    clip = 0;
                }

                DrawText(hDC, text, len, &rcText, DT_RIGHT | DT_TOP | clip);
            }
        }

        if (eexd->nTrailingWhitespaceWidth > 0)
        {
            const HLOCAL hText = Edit_GetHandle(hWnd);
            _ASSERT(hText);
            LPCTSTR lpText = (LPCTSTR) LocalLock(hText);
            _ASSERT(lpText);

            // TODO Use WM_CTLCOLORGUTTER
            const COLORREF colorWSEOL = RGB(255, 0, 0);
            const HBRUSH hBrush = GetStockBrush(DC_BRUSH);
            SetDCBrushColor(hDC, colorWSEOL);

            for (DWORD line = first; line < count; ++line)
            {
                const DWORD index = Edit_GetFileLineIndex(hWnd, line);
                const int length = Edit_GetFileLineLength(hWnd, index);
                if (length > 0 && lpText[index + length - 1] == TEXT(' ') || lpText[index + length - 1] == TEXT('\t'))
                {
                    const POINT ptPos = Edit_GetPosFromChar(hWnd, index);
                    const DWORD indexnext = Edit_GetFileLineIndex(hWnd, line + 1);
                    const POINT ptPosNext = Edit_GetPosFromChar(hWnd, indexnext);

                    const RECT rc = { rcClient.right - (LONG) eexd->nTrailingWhitespaceWidth, ptPos.y, rcClient.right, ptPosNext.y };
                    _VERIFY(FillRect(hDC, &rc, hBrush));
                }
            }

            LocalUnlock(hText);
        }

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
        rc.left -= eexd->nTrailingWhitespaceWidth;
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

void Edit_ReplaceLineEndings(HWND hWnd)
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

void InitEditEx(HWND hWnd)
{
    _ASSERT(GetWindowStyle(hWnd) & ES_MULTILINE);
    _VERIFY(SetWindowSubclass(hWnd, EditExProc, 0, reinterpret_cast<DWORD_PTR>(new EditExData({}))));
    Edit_SetExtendedStyle(hWnd, ES_EX_USETABS, ES_EX_USETABS);
}
