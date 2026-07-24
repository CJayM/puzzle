#include "states/SettingsState.h"
#include "states/MenuState.h"
#include "core/Game.h"
#include "core/Config.h"
#include "raylib.h"

#include <memory>

static Rectangle btnR(float x, float y, float w, float h) { return Rectangle{ x, y, w, h }; }
static bool hover(Rectangle r) { return CheckCollisionPointRec(GetMousePosition(), r); }

void SettingsState::Enter(Game&)
{
    m_musicOn = true;
}

void SettingsState::Exit(Game&)
{
}

void SettingsState::Update(Game& game)
{
    int sw = GetScreenWidth();
    int cX = sw / 2;
    int y = 180;

    Rectangle btnMusic = btnR((float)(cX - 100), (float)y, 200.0f, 50.0f);
    Rectangle btnBack  = btnR((float)(cX - 100), (float)(y + 190), 200.0f, 50.0f);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        if (hover(btnMusic))
            m_musicOn = !m_musicOn;
        if (hover(btnBack))
            game.ChangeState(std::make_unique<MenuState>());
    }
}

void SettingsState::Draw(Game&)
{
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    int cX = sw / 2;
    int y = 180;

    ClearBackground(Color{ 30, 33, 36, 255 });

    DrawText("Settings", cX - MeasureText("Settings", 40) / 2, 40, 40, WHITE);

    // Music toggle
    Rectangle btnMusic = btnR((float)(cX - 100), (float)y, 200.0f, 50.0f);
    Color musicBg = m_musicOn ? ColorAlpha(GREEN, 0.4f) : ColorAlpha(RED, 0.3f);
    Color musicBorder = m_musicOn ? GREEN : RED;
    DrawRectangleRec(btnMusic, musicBg);
    DrawRectangleLinesEx(btnMusic, 1, musicBorder);
    DrawText(m_musicOn ? "Music: ON" : "Music: OFF",
             cX - 40, y + 14, 18, WHITE);

    // Back
    Rectangle btnBack = btnR((float)(cX - 100), (float)(y + 190), 200.0f, 50.0f);
    DrawRectangleRec(btnBack, ColorAlpha(ORANGE, 0.3f));
    DrawRectangleLinesEx(btnBack, 1, ORANGE);
    DrawText("Back", cX - 25, y + 204, 18, WHITE);

    DrawText("(Music playback coming soon)",
             cX - 100, sh - 50, 14, GRAY);
}
