#pragma once
#include <windows.h>
#include <CommDlg.h>
#include "GdiPlus.h"
#include "Rad/WinError.h"

inline void NotifyParent(HWND hWnd, int code)
{
    HWND hWndParent = GetParent(hWnd);
    SendMessage(GetParent(hWnd), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(hWnd), code), (LPARAM) hWnd);
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

inline DWORD EditGetCursor(HWND hEdit)
{
#if 0
    // NOTE There is no message to get the cursor position
    // This works by unselecting the reselecting
    SetWindowRedraw(hEdit, FALSE);
    DWORD nSelStart, nSelend;
    EditGetSel(hEdit, &nSelStart, &nSelend);
    Edit_SetSel(hEdit, (DWORD) -1, (DWORD) -1);
    DWORD nCursor;
    EditGetSel(hEdit, &nCursor, nullptr);
    if (nCursor == nSelStart)
        Edit_SetSel(hEdit, nSelend, nSelStart);
    else
        Edit_SetSel(hEdit, nSelStart, nSelend);
    SetWindowRedraw(hEdit, TRUE);
    return nCursor;
#else
    const EditData* edp = EditGetData(hEdit);
    _ASSERT(edp->hText == Edit_GetHandle(hEdit)); // Ensure that the handle is correct
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

inline LRESULT CALLBACK EditExProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR /*uIdSubclass*/, DWORD_PTR /*dwRefData*/)
{
    static DWORD nPos = 0;
    LRESULT ret = DefSubclassProc(hWnd, uMsg, wParam, lParam);

    switch (uMsg)
    {
    case EM_SETSEL:
    case EM_UNDO:
    case WM_PASTE:
    case WM_KEYDOWN:
    case WM_LBUTTONDOWN:
        NotifyParent(hWnd, EN_SEL_CHANGED);
    break;
    case WM_MOUSEMOVE:
        if (wParam & MK_LBUTTON && nPos != (DWORD) lParam)
            NotifyParent(hWnd, EN_SEL_CHANGED);
        nPos = (DWORD) lParam;
        break;
    }

    return ret;
}
