#include "EditPlus.h"

namespace
{
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
        const HANDLE hText = Edit_GetHandle(hWnd);
        const SIZE_T size = CHECK_LE(GlobalSize(hText) / sizeof(TCHAR));
        LPTSTR lpText = (LPTSTR) CHECK_LE(GlobalLock(hText));
        if (lpText)
        {
            ModifyWhiteSpace(lpText, size, viewWhiteSpace);
            GlobalUnlock(hText);
            CHECK_LE(InvalidateRect(hWnd, NULL, TRUE));
        }
    }
}

struct EditExData
{
    DWORD nPos; // Last mouse position
    bool viewWhiteSpace = false; // Show whitespace
};

LRESULT EditExProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR dwRefData)
{
    EditExData* eexd = reinterpret_cast<EditExData*>(dwRefData);

    LRESULT ret = 0;

    switch (uMsg)
    {
    case EM_EX_GETCARET:
    {
        DWORD* nCaret = (DWORD*) wParam;
        *nCaret = EditGetCaret(hWnd);
        break;
    }
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
    case WM_CHAR:
        if (eexd->viewWhiteSpace)
            ModifyWhiteSpace(reinterpret_cast<LPTSTR>(&wParam), 1, true);
        ret = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        break;
    case WM_COPY:
    {
        DWORD nSelStart, nSelEnd;
        Edit_GetSelEx(hWnd, &nSelStart, &nSelEnd);
        if (nSelStart != nSelEnd)
        {
            const DWORD nLength = nSelEnd - nSelStart;
            const HANDLE hClip = CHECK_LE(GlobalAlloc(GMEM_MOVEABLE, (nLength + 1) * sizeof(TCHAR)));
            LPTSTR lpClip = (LPTSTR) CHECK_LE(GlobalLock(hClip));
            if (lpClip)
            {
                const HANDLE hText = Edit_GetHandle(hWnd);
                LPCTSTR lpText = (LPTSTR) CHECK_LE(GlobalLock(hText));
                if (lpText)
                {
                    CopyMemory(lpClip, lpText + nSelStart, nLength * sizeof(TCHAR));
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
        if (hClip)
        {
            LPTSTR lpClip = (LPTSTR) CHECK_LE(GlobalLock(hClip));
            if (lpClip)
            {
                const SIZE_T size = CHECK_LE(GlobalSize(hClip)) / sizeof(TCHAR);
                Edit_ReplaceSelEx(hWnd, lpClip, TRUE);
                GlobalUnlock(hClip);
            }
        }
        CHECK_LE(CloseClipboard());
        break;
    }
    case EM_REPLACESEL:
    {
        DWORD nSelStart, nSelEnd;
        Edit_GetSelEx(hWnd, &nSelStart, &nSelEnd);
        const DWORD dwSelStart = nSelStart;
        ret = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        if (eexd->viewWhiteSpace)
        {
            const HANDLE hText = Edit_GetHandle(hWnd);
            LPTSTR lpText = (LPTSTR) CHECK_LE(GlobalLock(hText));
            if (lpText)
            {
                DWORD nCaretPos;
                EditEx_GetCaret(hWnd, &nCaretPos);
                ModifyWhiteSpace(lpText + dwSelStart, nCaretPos - dwSelStart, true);
                GlobalUnlock(hText);
            }
            CHECK_LE(InvalidateRect(hWnd, NULL, TRUE));
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

void InitEditEx(HWND hWnd)
{
    CHECK_LE(SetWindowSubclass(hWnd, EditExProc, 0, reinterpret_cast<DWORD_PTR>(new EditExData({}))));
}
