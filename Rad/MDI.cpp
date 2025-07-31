#include "mdi.h"

void GetDefaultWndClass(WNDCLASS& wc);

extern HWND g_hWndMDIClient;

namespace
{
#define IDM_FILE_EXIT               1
#define IDM_WINDOW_TILE_VERTICAL    2
#define IDM_WINDOW_TILE_HORIZONTAL  3
#define IDM_WINDOW_CASCADE          4

    HMENU CreateDefaultMDIFrameMenu()
    {
        HMENU hMenu = CreateMenu();
        {
            HMENU hFileMenu = CreatePopupMenu();
            AppendMenu(hFileMenu, MF_STRING, IDM_FILE_EXIT, TEXT("Exit"));
            AppendMenu(hMenu, MF_POPUP, (UINT_PTR) hFileMenu, TEXT("File"));
        }
        {
            HMENU hWindowMenu = CreatePopupMenu();
            AppendMenu(hWindowMenu, MF_STRING, IDM_WINDOW_TILE_VERTICAL, TEXT("Tile Vertical"));
            AppendMenu(hWindowMenu, MF_STRING, IDM_WINDOW_TILE_HORIZONTAL, TEXT("Tile Horizontal"));
            AppendMenu(hWindowMenu, MF_STRING, IDM_WINDOW_CASCADE, TEXT("Cascade"));
            AppendMenu(hMenu, MF_POPUP | MF_RIGHTJUSTIFY, (UINT_PTR) hWindowMenu, TEXT("Window"));
        }
        return hMenu;
    }
}

LPCTSTR MainMDIFrameClass::ClassName() { return TEXT("RadMDIFrame"); }

void MainMDIFrameClass::GetWndClass(WNDCLASS& wc)
{
    GetDefaultWndClass(wc);
    wc.hbrBackground = GetSysColorBrush(COLOR_APPWORKSPACE);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.lpszMenuName = IDI_APPLICATION;
}

void MainMDIFrameClass::GetCreateWindow(CREATESTRUCT& cs)
{
    cs.style |= WS_OVERLAPPEDWINDOW;
}

HWND MDIFrame::CreateWnd(const CREATESTRUCT& ocs)
{
    CREATESTRUCT cs = ocs;
    if (cs.hMenu == NULL)
    {
        static HMENU hMenu = CreateDefaultMDIFrameMenu();
        cs.hMenu = hMenu;
    }
#if 1
    CreateWndParams cwp = { cs.lpCreateParams, this };
    return ::CreateWindowEx(cs.dwExStyle, cs.lpszClass, cs.lpszName, cs.style, cs.x, cs.y, cs.cx, cs.cy, cs.hwndParent, cs.hMenu, cs.hInstance, &cwp);
#else
    return WindowBase::CreateWnd(cs);
#endif
}


void MDIFrame::GetClientCreate(CLIENTCREATESTRUCT& ccs)
{
    HMENU hMenu = GetMenu(*this);
    ccs.hWindowMenu = GetSubMenu(hMenu, GetMenuItemCount(hMenu) - 1);
    ccs.idFirstChild = 1000;
}

LRESULT MDIFrame::HandleMessage(const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    if (uMsg == WM_COMMAND)
    {
        switch (LOWORD(wParam))
        {
        case IDM_FILE_EXIT:
            SetHandled(true);
            PostMessage(*this, WM_CLOSE, 0, 0);
            return 0;

        case IDM_WINDOW_TILE_VERTICAL:
            SetHandled(true);
            //SendMessage(GetMDIClient(), WM_MDITILE, 0, 0);
            TileWindows(GetMDIClient(), MDITILE_VERTICAL, nullptr, 0, nullptr);
            return 0;

        case IDM_WINDOW_TILE_HORIZONTAL:
            SetHandled(true);
            //SendMessage(GetMDIClient(), WM_MDITILE, 0, 0);
            TileWindows(GetMDIClient(), MDITILE_HORIZONTAL, nullptr, 0, nullptr);
            return 0;

        case IDM_WINDOW_CASCADE:
            SetHandled(true);
            //SendMessage(GetMDIClient(), WM_MDICASCADE, 0, 0);
            CascadeWindows(GetMDIClient(), 0, nullptr, 0, nullptr);
            return 0;
        }
    }

    return 0;
}

LRESULT MDIFrame::ProcessMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        CreateMDIClient();
        break;

    case WM_ACTIVATE:
        if (LOWORD(wParam))
            g_hWndMDIClient = GetMDIClient();
        else if (g_hWndMDIClient == GetMDIClient())
            g_hWndMDIClient = NULL;
        break;
    }

    const LRESULT ret = WindowBase::ProcessMessage(uMsg, wParam, lParam);

    if (IsHandled() && (uMsg == WM_COMMAND || uMsg == WM_MENUCHAR || uMsg == WM_SETFOCUS /*|| uMsg == WM_SIZE*/ || uMsg == WM_INITMENUPOPUP))
        DefFrameProc(*this, GetMDIClient(), uMsg, wParam, lParam);

    return IsHandled() ? ret : (SetHandled(true), DefFrameProc(*this, GetMDIClient(), uMsg, wParam, lParam));
}

void MDIFrame::CreateMDIClient()
{
    _ASSERT(m_hWndMDIClient == NULL);

    CLIENTCREATESTRUCT ccs = {};
    GetClientCreate(ccs);

    m_hWndMDIClient = CreateWindow(TEXT("MDICLIENT"), (LPCTSTR) NULL,
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_VSCROLL | WS_HSCROLL,
        0, 0, 0, 0, *this, (HMENU) 0, NULL, (LPSTR) &ccs);
}

HWND MDIChild::CreateWnd(const CREATESTRUCT& cs)
{
    _ASSERT(cs.dwExStyle == 0);
    _ASSERT(cs.hMenu == 0);
    CreateWndParams cwp = { cs.lpCreateParams, this };
    return ::CreateMDIWindow(cs.lpszClass, cs.lpszName, cs.style, cs.x, cs.y, cs.cx, cs.cy, cs.hwndParent, cs.hInstance, reinterpret_cast<LPARAM>(&cwp));
}

LRESULT MDIChild::HandleMessage(const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    return 0;
}

LRESULT MDIChild::ProcessMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    const LRESULT ret = WindowBase::ProcessMessage(uMsg, wParam, lParam);

    if (IsHandled() && (uMsg == WM_CHILDACTIVATE || uMsg == WM_GETMINMAXINFO || uMsg == WM_MENUCHAR || uMsg == WM_MOVE || uMsg == WM_SETFOCUS || uMsg == WM_SIZE || uMsg == WM_SYSCOMMAND))
        DefMDIChildProc(*this, uMsg, wParam, lParam);

    return IsHandled() ? ret : (SetHandled(true), DefMDIChildProc(*this, uMsg, wParam, lParam));
}
