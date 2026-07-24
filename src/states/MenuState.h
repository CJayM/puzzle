#ifndef MENUSTATE_H
#define MENUSTATE_H

#include "states/GameState.h"

class MenuState : public GameState
{
public:
    void Enter(Game& game) override;
    void Exit(Game& game) override;
    void Update(Game& game) override;
    void Draw(Game& game) override;
};

#endif // MENUSTATE_H
