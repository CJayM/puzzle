#ifndef MENUSTATE_H
#define MENUSTATE_H

#include "states/GameState.h"
#include "raylib.h"

class MenuState : public GameState
{
public:
    void Enter(Game& game) override;
    void Exit(Game& game) override;
    void Update(Game& game) override;
    void Draw(Game& game) override;

private:
    int m_selectedIndex = -1;
    int m_hoveredIndex = -1;
    int m_scrollOffset = 0;
    Texture2D m_preview = { 0 };
    int m_lastPreviewIndex = -1;
};

#endif // MENUSTATE_H
