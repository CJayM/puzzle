#ifndef DIFFICULTYSTATE_H
#define DIFFICULTYSTATE_H

#include "states/GameState.h"
#include "raylib.h"

struct DifficultyOption
{
    const char* label;
    int rows;
    int cols;
    int total;
};

class DifficultyState : public GameState
{
public:
    void Enter(Game& game) override;
    void Exit(Game& game) override;
    void Update(Game& game) override;
    void Draw(Game& game) override;

private:
    static const DifficultyOption s_options[6];

    Texture2D m_preview = { 0 };
    int m_selectedOption = 2; // default: 4x4
};

#endif // DIFFICULTYSTATE_H
