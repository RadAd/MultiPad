#pragma once

#include "Window.h"

class MDIChild;

//HWND CreateWnd(const CREATESTRUCT& cs, MDIChild* wnd);

struct MainMDIFrameClass
{
    static LPCTSTR ClassName();
    static void GetWndClass(WNDCLASS& wc);
    static void GetCreateWindow(CREATESTRUCT& cs);
};

class MDIFrame : public WindowBase
{
public:
    HWND GetMDIClient() const { return m_hWndMDIClient; }

protected:
    //virtual void GetCreateWindow(CREATESTRUCT& cs) override;
    virtual HWND CreateWnd(const CREATESTRUCT& cs);
    virtual void GetClientCreate(CLIENTCREATESTRUCT& ccs);
    virtual LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override;
    virtual LRESULT ProcessMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, bool& bHandled) override;
    //virtual void PostHandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override;

private:
    void CreateMDIClient();

    HWND m_hWndMDIClient = NULL;
};

class MDIChild : public WindowBase
{
protected:
    virtual HWND CreateWnd(const CREATESTRUCT& cs);
    virtual LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override;
    virtual LRESULT ProcessMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, bool& bHandled) override;
    //virtual void PostHandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override;
};
