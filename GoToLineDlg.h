#pragma once

#include "Rad/Dialog.h"

class GoToLineDlg : public Dialog
{
public:
    static DWORD DoModal(HWND hWndParent);

protected:
    virtual LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override;

    void OnCommand(int id, HWND hWndCtl, UINT codeNotify);

private:
    int m_Line = 0;
};
