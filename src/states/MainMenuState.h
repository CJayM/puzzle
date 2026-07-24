#ifndef MAINMENUSTATE_H
#define MAINMENUSTATE_H

#include "states/GameState.h"

class MainMenuState : public GameState
{
public:
    void Enter(Game& game) override;
    void Exit(Game& game) override;
    void Update(Game& game) override;
    void Draw(Game& game) override;

private:
    bool m_hasContinue = false;
};

#endif
