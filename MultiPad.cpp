#include "Rad/MDI.h"
#include "Rad/Dialog.h"
#include "Rad/Windowxx.h"
#include "Rad/WinError.h"
#include <CommCtrl.h>
//#include <tchar.h>
//#include <strsafe.h>
#include "resource.h"

const TCHAR* g_ProjectName = TEXT("MultiPad");
const TCHAR* g_ProjectTitle = TEXT("MultiPad");

inline LONG Width(const RECT r)
{
    return r.right - r.left;
}

inline LONG Height(const RECT r)
{
    return r.bottom - r.top;
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

HFONT CreateFont()
{
    static HFONT hFont = CreateFont(
        16, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        TEXT("Consolas"));
    CHECK_LE(hFont != NULL);
    return hFont;
}

class TextDocWindow : public MDIChild
{
    friend WindowManager<TextDocWindow>;
    using Class = ChildClass;
public:
    static ATOM Register() { return ::Register<Class>(); }
    static TextDocWindow* Create(HWND hWndParent, BOOL Maximized) { return WindowManager<TextDocWindow>::Create(hWndParent, TEXT("New Document"), (LPVOID) (INT_PTR) Maximized); }

protected:
    virtual HWND CreateWnd(const CREATESTRUCT& ocs)
    {
        BOOL Maximized = (BOOL) (INT_PTR) ocs.lpCreateParams;
        CREATESTRUCT cs = ocs;
        if (Maximized)
            cs.style |= WS_MAXIMIZE;
        return MDIChild::CreateWnd(cs);
    }

    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override
    {
        LRESULT ret = 0;
        switch (uMsg)
        {
            HANDLE_MSG(WM_CREATE, OnCreate);
            HANDLE_MSG(WM_DESTROY, OnDestroy);
            HANDLE_MSG(WM_COMMAND, OnCommand);
            HANDLE_MSG(WM_SIZE, OnSize);
            HANDLE_MSG(WM_SETFOCUS, OnSetFocus);
            HANDLE_MSG(WM_INITMENUPOPUP, OnInitMenuPopup);
        }

        if (!IsHandled())
            ret = MDIChild::HandleMessage(uMsg, wParam, lParam);

        return ret;
    }

private:
    BOOL OnCreate(LPCREATESTRUCT lpCreateStruct);
    void OnDestroy();
    void OnCommand(int id, HWND hWndCtl, UINT codeNotify);
    void OnSize(UINT state, int cx, int cy);
    void OnSetFocus(HWND hwndOldFocus);
    void OnInitMenuPopup(HMENU hMenu, UINT item, BOOL fSystemMenu);

    HWND m_hWndChild = NULL;
};

BOOL TextDocWindow::OnCreate(const LPCREATESTRUCT lpCreateStruct)
{
    m_hWndChild = Edit_Create(*this, WS_CHILD | WS_VISIBLE | ES_MULTILINE, RECT(), 0);
    SetWindowFont(m_hWndChild, CreateFont(), TRUE);
    return TRUE;
}

void TextDocWindow::OnDestroy()
{
}

void TextDocWindow::OnCommand(int id, HWND hWndCtl, UINT codeNotify)
{
    switch (id)
    {
    case ID_FILE_CLOSE:
        SendMessage(*this, WM_CLOSE, 0, 0);
        // TODO Should we use WM_MDIDESTROY instead?
        break;
    default:
        SetHandled(false);
        break;
    }
}

void TextDocWindow::OnSize(const UINT state, const int cx, const int cy)
{
    if (m_hWndChild)
    {
        SetWindowPos(m_hWndChild, NULL, 0, 0,
            cx, cy,
            SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void TextDocWindow::OnSetFocus(const HWND hwndOldFocus)
{
    if (m_hWndChild)
        SetFocus(m_hWndChild);
}

void TextDocWindow::OnInitMenuPopup(HMENU hMenu, UINT item, BOOL fSystemMenu)
{
    const int count = GetMenuItemCount(hMenu);

    MENUITEMINFO mii = {};
    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_STATE | MIIM_ID;
    for (int i = 0; i < count; ++i)
    {
        CHECK_LE(GetMenuItemInfo(hMenu, i, TRUE, &mii));
        switch (mii.wID)
        {
        case ID_FILE_SAVE:
            mii.fState = MFS_DISABLED;
            SetMenuItemInfo(hMenu, i, TRUE, &mii);
            break;
        }
    }
}

class RootWindow : public MDIFrame
{
    friend WindowManager<RootWindow>;
    using Class = MainMDIFrameClass;
public:
    static ATOM Register() { return ::Register<Class>(); }
    static RootWindow* Create() { return WindowManager<RootWindow>::Create(NULL, g_ProjectTitle); }

protected:
    virtual HWND CreateWnd(const CREATESTRUCT& ocs)
    {
        CREATESTRUCT cs = ocs;
        cs.hMenu = LoadMenu(cs.hInstance, MAKEINTRESOURCE(IDR_MENU1));
        return MDIFrame::CreateWnd(cs);
    }

    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override
    {
        LRESULT ret = 0;
        switch (uMsg)
        {
            HANDLE_MSG(WM_CREATE, OnCreate);
            HANDLE_MSG(WM_DESTROY, OnDestroy);
            HANDLE_MSG(WM_COMMAND, OnCommand);
            HANDLE_MSG(WM_INITMENUPOPUP, OnInitMenuPopup);
        }

        if (!IsHandled())
            ret = MDIFrame::HandleMessage(uMsg, wParam, lParam);

        return ret;
    }

    HWND GetActiveChild(BOOL* b = nullptr) const
    {
        HWND hWndChild = (HWND) SendMessage(GetMDIClient(), WM_MDIGETACTIVE, 0, (INT_PTR) b);
        return hWndChild;
    }

private:
    BOOL OnCreate(LPCREATESTRUCT lpCreateStruct);
    void OnDestroy();
    void OnCommand(int id, HWND hWndCtl, UINT codeNotify);
    void OnInitMenuPopup(HMENU hMenu, UINT item, BOOL fSystemMenu);
};

BOOL RootWindow::OnCreate(const LPCREATESTRUCT lpCreateStruct)
{
    return TRUE;
}

void RootWindow::OnDestroy()
{
    PostQuitMessage(0);
}

void RootWindow::OnCommand(int id, HWND hWndCtl, UINT codeNotify)
{
    switch (id)
    {
    case ID_FILE_NEW:
    {
        BOOL bMaximized = FALSE;
        if (GetActiveChild(&bMaximized) == NULL)
            bMaximized = TRUE;
        CHECK_LE(TextDocWindow::Create(GetMDIClient(), bMaximized));
        break;
    }
    case ID_FILE_OPEN:
        //RadDialog::Create<OpenFileDialog>(*this);
        break;
    case ID_FILE_EXIT:
        SendMessage(*this, WM_CLOSE, 0, 0);
        break;
    case ID_WINDOW_TILEVERTICAL:
        TileWindows(GetMDIClient(), MDITILE_VERTICAL, nullptr, 0, nullptr);
        break;
    case ID_WINDOW_TILEHORIZONTAL:
        TileWindows(GetMDIClient(), MDITILE_HORIZONTAL, nullptr, 0, nullptr);
        break;
    case ID_WINDOW_CASCADE:
        CascadeWindows(GetMDIClient(), 0, nullptr, 0, nullptr);
        break;
    default:
    {
        HWND hWndActive = GetActiveChild();
        if (hWndActive)
            FORWARD_WM_COMMAND(hWndActive, id, hWndCtl, codeNotify, SendMessage);
        SetHandled(false);
        break;
    }
    }
}

void RootWindow::OnInitMenuPopup(HMENU hMenu, UINT item, BOOL fSystemMenu)
{
    const HWND hWndActive = GetActiveChild();

    MENUITEMINFO mii = {};
    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_STATE | MIIM_ID;
    const int count = GetMenuItemCount(hMenu);
    for (int i = 0; i < count; ++i)
    {
        CHECK_LE(GetMenuItemInfo(hMenu, i, TRUE, &mii));
        switch (mii.wID)
        {
        case ID_FILE_SAVE:
        case ID_FILE_CLOSE:
        case ID_WINDOW_TILEHORIZONTAL:
        case ID_WINDOW_TILEVERTICAL:
        case ID_WINDOW_CASCADE:
            mii.fState = hWndActive ? MFS_ENABLED : MFS_DISABLED;
            SetMenuItemInfo(hMenu, i, TRUE, &mii);
            break;
        }
    }

    if (hWndActive)
        FORWARD_WM_INITMENUPOPUP(hWndActive, hMenu, item, fSystemMenu, SendMessage);
}

bool Run(_In_ const LPCTSTR lpCmdLine, _In_ const int nShowCmd)
{
    RadLogInitWnd(NULL, "MultiPad", L"MultiPad");

    CHECK_LE_RET(RootWindow::Register(), false);
    CHECK_LE_RET(TextDocWindow::Register(), false);

    RootWindow* prw = CHECK_LE(RootWindow::Create());
    if (!prw) return false;

    RadLogInitWnd(*prw, nullptr, nullptr);
    ShowWindow(*prw, nShowCmd);

    return true;
}
