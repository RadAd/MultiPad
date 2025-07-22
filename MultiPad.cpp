#include "Rad/Window.h"
#include "Rad/Dialog.h"
#include "Rad/Windowxx.h"
#include "Rad/WinError.h"
//#include <tchar.h>
//#include <strsafe.h>
//#include "resource.h"
#define IDD_DIALOG1                     101

const TCHAR* g_ProjectName = TEXT("MultiPad");
const TCHAR* g_ProjectTitle = TEXT("MultiPad");

class RootWindow : public Window
{
    friend WindowManager<RootWindow>;
    using Class = MainClass;
public:
    static ATOM Register() { return ::Register<Class>(); }
    static RootWindow* Create() { return WindowManager<RootWindow>::Create(NULL, g_ProjectTitle); }

protected:
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override
    {
        LRESULT ret = 0;
        switch (uMsg)
        {
            HANDLE_MSG(WM_CREATE, OnCreate);
            HANDLE_MSG(WM_DESTROY, OnDestroy);
            HANDLE_MSG(WM_SIZE, OnSize);
            HANDLE_MSG(WM_SETFOCUS, OnSetFocus);
        }

        if (!IsHandled())
            ret = Window::HandleMessage(uMsg, wParam, lParam);

        return ret;
    }

private:
    BOOL OnCreate(LPCREATESTRUCT lpCreateStruct);
    void OnDestroy();
    void OnSize(UINT state, int cx, int cy);
    void OnSetFocus(HWND hwndOldFocus);

    HWND m_hWndChild = NULL;
};

BOOL RootWindow::OnCreate(const LPCREATESTRUCT lpCreateStruct)
{
    return TRUE;
}

void RootWindow::OnDestroy()
{
    PostQuitMessage(0);
}

void RootWindow::OnSize(const UINT state, const int cx, const int cy)
{
    if (m_hWndChild)
    {
        SetWindowPos(m_hWndChild, NULL, 0, 0,
            cx, cy,
            SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void RootWindow::OnSetFocus(const HWND hwndOldFocus)
{
    if (m_hWndChild)
        SetFocus(m_hWndChild);
}

class RootDialog : public Dialog
{
    friend DialogManager<RootDialog>;
public:
    static void GetCreateDlg(DLGCREATESTRUCT& cs)
    {
        Dialog::GetCreateDlg(cs);
        cs.lpTemplateName = MAKEINTRESOURCE(IDD_DIALOG1);
    }
    static RootDialog* Create() { return DialogManager<RootDialog>::Create(NULL); }
    static INT_PTR DoModal() { RootDialog dlg; return DialogManager<RootDialog>::DoModal(dlg, NULL); }

protected:
    using Dialog::DoModal;

    virtual LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override
    {
        LRESULT ret = 0;
        switch (uMsg)
        {
            HANDLE_MSG(WM_INITDIALOG, OnInitDialog);
            HANDLE_MSG(WM_DESTROY, OnDestroy);
            HANDLE_MSG(WM_COMMAND, OnCommand);
        }

        return IsHandled() ? ret : Dialog::HandleMessage(uMsg, wParam, lParam);
    }

private:
    BOOL OnInitDialog(HWND hwndFocus, LPARAM lParam);
    void OnDestroy();
    void OnCommand(int id, HWND hwndCtl, UINT codeNotify);
};

BOOL RootDialog::OnInitDialog(HWND hwndFocus, LPARAM lParam)
{
    //ShowWindow(*this, SW_SHOW);
    return TRUE;
}

void RootDialog::OnDestroy()
{
    if (!IsModal())
        PostQuitMessage(0);
}

void RootDialog::OnCommand(int id, HWND hWndCtl, UINT codeNotify)
{
    switch (id)
    {
    case IDOK:
    case IDCANCEL:
        if (IsModal())
            EndDialog(*this, id);
        else
            DestroyWindow(*this);
        break;

    default:
        SetHandled(false);
        break;
    }
}

bool Run(_In_ const LPCTSTR lpCmdLine, _In_ const int nShowCmd)
{
    RadLogInitWnd(NULL, "MultiPad", L"MultiPad");

    if (false)
    {
        INT_PTR r = RootDialog::DoModal();
        CHECK_LE(r >= 0);
        if (r < 0) return false;
    }
    if (false)
    {
        RootDialog* prd = CHECK_LE(RootDialog::Create());
        if (!prd) return false;

        RadLogInitWnd(*prd, nullptr, nullptr);
        ShowWindow(*prd, nShowCmd);
    }
    if (true)
    {
        CHECK_LE_RET(RootWindow::Register(), false);

        RootWindow* prw = CHECK_LE(RootWindow::Create());
        if (!prw) return false;

        RadLogInitWnd(*prw, nullptr, nullptr);
        ShowWindow(*prw, nShowCmd);
    }
    return true;
}
