#include "MessageHandler.h"
#include <algorithm>

#if _UNICODE
#define _RPTFT0 _RPTFW0
#else
#define _RPTFT0 _RPTF0
#endif

namespace
{
    inline bool IsDialog(const HWND hWnd)
    {
        return WC_DIALOG == MAKEINTATOM(GetClassLong(hWnd, GCW_ATOM));
    }
}

LRESULT MessageHandler::ProcessMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, bool& bHandled)
{
    Message m = { uMsg, wParam, lParam, false };
    Message* const pMsg = std::exchange(m_msg, &m);

    LRESULT ret = 0;
    try
    {
        ret = HandleMessage(uMsg, wParam, lParam);
    }
    catch (...)
    {
        _RPTFT0(_CRT_ERROR, TEXT("Unhandled exception"));
    }

    _ASSERTE(m_msg == &m);
    std::exchange(m_msg, pMsg);

    bHandled = m.m_bHandled;
    return ret;
}

MessageHandler* MessageHandler::GetFrom(HWND hWnd)
{
    return reinterpret_cast<MessageHandler*>(GetWindowLongPtr(hWnd, IsDialog(hWnd) ? DWLP_USER : GWLP_USERDATA));
}

LRESULT CALLBACK MessageHandler::s_WndProc(const HWND hWnd, const UINT uMsg, const WPARAM wParam, /*const*/ LPARAM lParam, bool& bHandled)
{
    MessageHandler* self = GetFrom(hWnd);
    if (!self)
        return 0;

    const LRESULT ret = self->ProcessMessage(uMsg, wParam, lParam, bHandled);
    //_ASSERT(bHandled);

    if (self->m_bDelete && self->m_msg == nullptr)
        delete self;

    return ret;
}

LRESULT MessageChain::ProcessMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool& bHandled)
{
    Message m = { hWnd, uMsg, wParam, lParam, false };
    Message* const pMsg = std::exchange(m_msg, &m);

    LRESULT ret = 0;
    try
    {
        ret = HandleMessage(uMsg, wParam, lParam);
    }
    catch (...)
    {
        _RPTFT0(_CRT_ERROR, TEXT("Unhandled exception"));
    }

    _ASSERTE(m_msg == &m);
    std::exchange(m_msg, pMsg);

    bHandled = m.m_bHandled;
    return ret;
}
