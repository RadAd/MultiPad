#pragma once
#include "Rad/MessageHandler.h"

class CommandState
{
public:
    struct State
    {
        bool enabled;
        bool checked;
    };

    virtual void GetState(UINT id, State& state) const = 0;
};

class CommandStateChain : public MessageChain
{
public:
    void Init(CommandState* state)
    {
        m_pState = state;
    }

protected:
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override;

private:
    void OnInitMenuPopup(HMENU hMenu, UINT item, BOOL fSystemMenu);

    CommandState* m_pState = nullptr;
};
