#pragma once

#include "MessageHandler.h"

struct DLGCREATESTRUCT
{
    HINSTANCE hInstance;
    DLGPROC lpDialogFunc;
    LPCTSTR lpTemplateName;
    HWND hWndParent;
    LPARAM dwInitParam;
};

template<class T>
class DialogManager
{
public:
    static T* Create(HWND hWndParent, LPARAM dwInitParam = 0)
    {
        DLGCREATESTRUCT cs = {};
        cs.hWndParent = hWndParent;
        cs.dwInitParam = dwInitParam;
        T::GetCreateDlg(cs);
        return Create(cs);
    }

    static T* Create(DLGCREATESTRUCT& cs)
    {
        T* self = new DEBUG_NEW T();
        if (self && self->CreateDlg(cs) != NULL)
            return self;
        else
        {
            delete self;
            return nullptr;
        }
    }

    static INT_PTR DoModal(T& dlg, HWND hWndParent, LPARAM dwInitParam = 0)
    {
        DLGCREATESTRUCT cs = {};
        cs.hWndParent = hWndParent;
        cs.dwInitParam = dwInitParam;
        T::GetCreateDlg(cs);
        return dlg.DoModal(cs);
    }
};

class Dialog : public MessageHandler
{
public:
    bool IsModal() const { return m_bModal; }

protected:
    static void GetCreateDlg(DLGCREATESTRUCT& cs);
    virtual HWND CreateDlg(const DLGCREATESTRUCT& cs);
    virtual INT_PTR DoModal(const DLGCREATESTRUCT& cs);

    virtual LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override;
    virtual LRESULT ProcessMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, bool& bHandled) override;

private:
    static void Store(HWND hWnd, MessageHandler* dlg)
    {
        SetWindowLongPtr(hWnd, DWLP_USER, reinterpret_cast<LONG_PTR>(dlg));
    }

    static INT_PTR CALLBACK s_DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    bool m_bModal = false;
};
