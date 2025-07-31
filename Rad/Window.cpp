#include "Window.h"
#include <windowsx.h>

extern HINSTANCE g_hInstance; 

namespace
{
    inline bool IsMDIChild(const HWND hWnd)
    {
        return (GetWindowExStyle(hWnd) & WS_EX_MDICHILD) != 0;
    }
}

void GetDefaultWndClass(WNDCLASS& wc)
{
    wc.style |= CS_DBLCLKS;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    //wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
}

LPCTSTR MainClass::ClassName() { return TEXT("RadMain"); }

void MainClass::GetWndClass(WNDCLASS& wc)
{
    GetDefaultWndClass(wc);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
}

void MainClass::GetCreateWindow(CREATESTRUCT& cs)
{
    cs.style |= WS_OVERLAPPEDWINDOW;
}

LPCTSTR ChildClass::ClassName() { return TEXT("RadChild"); }

void ChildClass::GetWndClass(WNDCLASS& wc)
{
    GetDefaultWndClass(wc);
}

void ChildClass::GetCreateWindow(CREATESTRUCT& cs)
{
    cs.style |= WS_CHILD | WS_VISIBLE;
}

HWND Window::CreateWnd(const CREATESTRUCT& cs)
{
    CreateWndParams cwp = { cs.lpCreateParams, this };
    return ::CreateWindowEx(cs.dwExStyle, cs.lpszClass, cs.lpszName, cs.style, cs.x, cs.y, cs.cx, cs.cy, cs.hwndParent, cs.hMenu, cs.hInstance, &cwp);
}

void WindowBase::GetWndClass(WNDCLASS& wc)
{
    wc.lpfnWndProc = s_WndProc;
    wc.hInstance = g_hInstance;
}

LRESULT WindowBase::ProcessMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    const LRESULT ret = MessageHandler::ProcessMessage(uMsg, wParam, lParam);

    if (uMsg == WM_NCDESTROY)
    {
        Store(*this, nullptr);
        Delete();
    }

    return ret;
}

WindowBase* WindowBase::AdjustCreateParam(const HWND hWnd, const LPARAM lParam)
{
    const LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
    if (IsMDIChild(hWnd))
    {
        const LPMDICREATESTRUCT mdics = reinterpret_cast<LPMDICREATESTRUCT>(lpcs->lpCreateParams);
        const CreateWndParams* cwp = reinterpret_cast<CreateWndParams*>(mdics->lParam);
        //mdics->lParam = reinterpret_cast<LPARAM>(cwp->lpCreateParams);
        lpcs->lpCreateParams = cwp->lpCreateParams;
        return cwp->wnd;
    }
    else
    {
        const CreateWndParams* cwp = reinterpret_cast<CreateWndParams*>(lpcs->lpCreateParams);
        lpcs->lpCreateParams = cwp->lpCreateParams;
        return cwp->wnd;
    }
}

LRESULT CALLBACK WindowBase::s_WndProc(const HWND hWnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    if (uMsg == WM_NCCREATE)
    {
        WindowBase* self = AdjustCreateParam(hWnd, lParam);
        self->Set(hWnd);
        Store(hWnd, self);
    }
    else if (uMsg == WM_CREATE)
    {
        /*WindowBase* self =*/ AdjustCreateParam(hWnd, lParam);
        //_ASERTE(self == Retrieve(hWnd));
    }

    bool bHandled = false;
    const LRESULT ret = MessageHandler::s_WndProc(hWnd, uMsg, wParam, lParam, bHandled);

    if (uMsg == WM_CREATE && ret == -1)
    {
        // This will get deleted later
        //self->Set(NULL);
        Store(hWnd, nullptr);
    }

    return bHandled ? ret : DefWindowProc(hWnd, uMsg, wParam, lParam);
}

LRESULT Window::HandleMessage(const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_PAINT:
        SetHandled(true);
        OnPaint();
        return 0;
    case WM_PRINTCLIENT:
        SetHandled(true);
        OnPrintClient(reinterpret_cast<HDC>(wParam));
        return 0;
    default:
        return 0;
    }
}

LRESULT Window::ProcessMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    const LRESULT ret = WindowBase::ProcessMessage(uMsg, wParam, lParam);

    return IsHandled() ? ret : (SetHandled(true), DefWindowProc(*this, uMsg, wParam, lParam));
}

void Window::OnPaint()
{
    PAINTSTRUCT ps;
    BeginPaint(*this, &ps);
    OnDraw(&ps);
    EndPaint(*this, &ps);
}

void Window::OnPrintClient(const HDC hdc)
{
    PAINTSTRUCT ps = {};
    ps.hdc = hdc;
    GetClientRect(*this, &ps.rcPaint);
    OnDraw(&ps);
}
