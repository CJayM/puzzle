#include "states/MenuState.h"
#include "core/Game.h"
#include "raylib.h"

void MenuState::Enter(Game&)
{
}

void MenuState::Exit(Game&)
{
}

void MenuState::Update(Game&)
{
}

void MenuState::Draw(Game&)
{
    ClearBackground(DARKGREEN);
    DrawText("MenuState — choose image (coming soon)", 100, 100, 20, WHITE);
}
