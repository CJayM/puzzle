#ifndef COMPLETEDSTATE_H
#define COMPLETEDSTATE_H

#include "states/GameState.h"
#include "raylib.h"

class CompletedState : public GameState
{
public:
    void Enter(Game& game) override;
    void Exit(Game& game) override;
    void Update(Game& game) override;
    void Draw(Game& game) override;

private:
    Texture2D m_completedTexture = { 0 };
};

#endif // COMPLETEDSTATE_H
