#ifndef MENUSTATE_H
#define MENUSTATE_H

#include "states/GameState.h"
#include "raylib.h"

struct DifficultyPreset
{
    const char* label;
    int rows;
    int cols;
    int total;
};

class MenuState : public GameState
{
public:
    void Enter(Game& game) override;
    void Exit(Game& game) override;
    void Update(Game& game) override;
    void Draw(Game& game) override;

private:
    static const DifficultyPreset s_presets[4];

    int m_selectedIndex = -1;
    int m_hoveredIndex = -1;
    int m_scrollOffset = 0;
    int m_selectedPreset = 2; // default: 8x8 = 64
    Texture2D m_preview = { 0 };
    int m_lastPreviewIndex = -1;
};

#endif // MENUSTATE_H
