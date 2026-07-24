#ifndef SETTINGSSTATE_H
#define SETTINGSSTATE_H

#include "states/GameState.h"
#include "raylib.h"

class SettingsState : public GameState
{
public:
    void Enter(Game& game) override;
    void Exit(Game& game) override;
    void Update(Game& game) override;
    void Draw(Game& game) override;

private:
    bool m_musicOn = true;
};

#endif
