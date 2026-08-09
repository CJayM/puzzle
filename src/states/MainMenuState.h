#ifndef MAINMENUSTATE_H
#define MAINMENUSTATE_H

#include "states/GameState.h"
#include "ui/Button.h"

class MainMenuState : public GameState
{
public:
    void Enter(Game& game) override;
    void Exit(Game& game) override;
    void Update(Game& game) override;
    void Draw(Game& game) override;

private:
    void InitButtons();
    void UpdateButtons(); // refresh enabled state + recompute layout

    bool m_hasContinue = false;

    // Menu buttons: created once (Enter), laid out vertically
    ui::VLayout m_layout;
    ui::Button m_btnContinue, m_btnChoose, m_btnSettings, m_btnExit;
};

#endif
