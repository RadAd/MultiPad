#include "Dialog.h"
#include "WinError.h"
#include <windowsx.h>

extern HINSTANCE g_hInstance;
extern HWND g_hWndDlg;

namespace
{
    struct CreateDlgParams
    {
        LPARAM dwInitParam;
        Dialog* dlg;
    };
}

void Dialog::GetCreateDlg(DLGCREATESTRUCT& cs)
{
    cs.hInstance = g_hInstance;
    cs.lpDialogFunc = s_DlgProc;
}

HWND Dialog::CreateDlg(const DLGCREATESTRUCT& cs)
{
    CreateDlgParams p = { cs.dwInitParam, this };
    m_bModal = false;
    const HWND hDlg = CHECK_LE(CreateDialogParam(cs.hInstance, cs.lpTemplateName, cs.hWndParent, cs.lpDialogFunc, (LPARAM) &p));
    return hDlg;
}

INT_PTR Dialog::DoModal(const DLGCREATESTRUCT& cs)
{
    CreateDlgParams p = { cs.dwInitParam, this };
    m_bModal = true;
    const INT_PTR ret = DialogBoxParam(cs.hInstance, cs.lpTemplateName, cs.hWndParent, cs.lpDialogFunc, (LPARAM) &p);
    CHECK_LE(ret != -1);
    return ret;
}

LRESULT Dialog::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    return 0;
}

LRESULT Dialog::ProcessMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    const LRESULT ret = MessageHandler::ProcessMessage(uMsg, wParam, lParam);

    switch (uMsg)
    {
    case WM_ACTIVATE:
        if (LOWORD(wParam))
            g_hWndDlg = *this;
        else if (g_hWndDlg == *this)
            g_hWndDlg = NULL;
        break;

    case WM_NCDESTROY:
        if (!m_bModal)
        {
            Store(*this, nullptr);
            Delete();
        }
        break;
    }

    return ret;
}

INT_PTR CALLBACK Dialog::s_DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_INITDIALOG)
    {
        CreateDlgParams* cdp = reinterpret_cast<CreateDlgParams*>(lParam);
        lParam = cdp->dwInitParam;

        Dialog* self = cdp->dlg;
        self->Set(hWnd);
        Store(hWnd, self);
    }

    bool bHandled = false;
    const LRESULT ret = s_WndProc(hWnd, uMsg, wParam, lParam, bHandled);
    return bHandled ? SetDlgMsgResult(hWnd, uMsg, ret) : FALSE;
}
