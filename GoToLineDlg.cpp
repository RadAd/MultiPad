#include "GoToLineDlg.h"

#include "Rad/Windowxx.h"

#include <tchar.h>

#include "resource.h"

extern HINSTANCE g_hInstance;

DWORD GoToLineDlg::DoModal(HWND hWndParent)
{
    GoToLineDlg dlg;

    DLGCREATESTRUCT dcs = {};
    GetCreateDlg(dcs);
    dcs.lpTemplateName = MAKEINTRESOURCE(IDD_GOTOLINE);
    dcs.hWndParent = hWndParent;

    if (dlg.Dialog::DoModal(dcs) == IDOK)
        return dlg.m_Line;
    else
        return -1;
}

LRESULT GoToLineDlg::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LRESULT ret = 0;
    switch (uMsg)
    {
        HANDLE_MSG(WM_COMMAND, OnCommand);
    }
    if (!IsHandled())
        ret = Dialog::HandleMessage(uMsg, wParam, lParam);
    return ret;
}

void GoToLineDlg::OnCommand(int id, HWND hWndCtl, UINT codeNotify)
{
    switch (id)
    {
    case IDOK:
        m_Line = GetDlgItemInt(GetHWND(), IDC_LINE, NULL, FALSE);
        EndDialog(*this, IDOK);
        break;
    case IDCANCEL:
        EndDialog(*this, IDCANCEL);
        break;
    default:
        SetHandled(false);
        break;
    }
}
