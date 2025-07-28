#include "CommandStateChain.h"
#include "Rad/Windowxx.h"
#include "Rad/WinError.h"

namespace
{
    inline bool SetFlag(UINT& v, UINT mask, UINT n)
    {
        const UINT o = v;
        _ASSERT((~mask & n) == 0);
        v = (~mask & v) | (mask & n);
        return o != v;
    }
}

LRESULT CommandStateChain::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LRESULT ret = 0;
    switch (uMsg)
    {
        HANDLE_MSG(WM_INITMENUPOPUP, OnInitMenuPopup);
    }
    return ret;
}

void CommandStateChain::OnInitMenuPopup(HMENU hMenu, UINT item, BOOL fSystemMenu)
{
    if (fSystemMenu || GetMenuItemID(hMenu, 0) == SC_RESTORE)
    {
        SetHandled(false);
        return;
    }

    const int count = GetMenuItemCount(hMenu);

    MENUITEMINFO mii = {};
    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_STATE | MIIM_ID;
    for (int i = 0; i < count; ++i)
    {
        CHECK_LE(GetMenuItemInfo(hMenu, i, TRUE, &mii));

        if (mii.wID == 0xFFFF)    // SubMenu
            continue;

        CommandState::State state = { !(mii.fState & MFS_DISABLED), bool(mii.fState & MFS_CHECKED) };
        m_pState->GetState(mii.wID, state);
        if (SetFlag(mii.fState, MFS_ENABLED | MFS_DISABLED, state.enabled ? MFS_ENABLED : MFS_DISABLED))
            SetMenuItemInfo(hMenu, i, TRUE, &mii);
        if (SetFlag(mii.fState, MFS_CHECKED | MFS_UNCHECKED, state.checked ? MFS_CHECKED : MFS_UNCHECKED))
            SetMenuItemInfo(hMenu, i, TRUE, &mii);
    }
}
