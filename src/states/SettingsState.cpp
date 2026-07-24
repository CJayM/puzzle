#include "states/SettingsState.h"
#include "states/MainMenuState.h"
#include "core/Game.h"
#include "core/DrawHelper.h"
#include "raylib.h"

#include <cstdio>
#include <memory>

static SettingsData s_settings;

SettingsData& SettingsState::GetSettings() { return s_settings; }

void SettingsState::LoadFromFile()
{
    FILE* f = fopen("settings.dat", "rb");
    if (!f) return;
    fread(&s_settings, sizeof(s_settings), 1, f);
    fclose(f);
}

void SettingsState::SaveToFile()
{
    FILE* f = fopen("settings.dat", "wb");
    if (!f) return;
    fwrite(&s_settings, sizeof(s_settings), 1, f);
    fclose(f);
}

static const char* s_diffLabels[4] = {
    "25 pieces  (5x5)",
    "64 pieces  (8x8)",
    "256 pieces  (16x16)",
    "1024 pieces  (32x32)",
};

static Rectangle btnR(float x, float y, float w, float h) { return Rectangle{ x, y, w, h }; }
static bool hov(Rectangle r) { return CheckCollisionPointRec(GetMousePosition(), r); }

void SettingsState::Enter(Game&)
{
    m_musicOn = s_settings.musicOn;
    m_difficultyPreset = s_settings.difficultyPreset;
}

void SettingsState::Exit(Game&)
{
    s_settings.musicOn = m_musicOn;
    s_settings.difficultyPreset = m_difficultyPreset;
    SaveToFile();
}

void SettingsState::Update(Game& game)
{
    int sw = GetScreenWidth();
    int cX = sw / 2;
    int y = 180;
    int btnW = 260;
    int btnH = 44;
    int gap = 6;

    Rectangle rMusic = btnR((float)(cX - btnW / 2), (float)y, (float)btnW, (float)btnH);
    int diffY = y + btnH + 30;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        if (hov(rMusic))
            m_musicOn = !m_musicOn;

        for (int i = 0; i < 4; ++i)
        {
            Rectangle r = btnR((float)(cX - btnW / 2), (float)(diffY + i * (btnH + gap)),
                               (float)btnW, (float)btnH);
            if (hov(r)) { m_difficultyPreset = i; break; }
        }

        // Back
        Rectangle rBack = btnR((float)(cX - 100), (float)(diffY + 4 * (btnH + gap) + 20),
                               200.0f, 50.0f);
        if (hov(rBack))
            game.ChangeState(std::make_unique<MainMenuState>());
    }
}

void SettingsState::Draw(Game& game)
{
    int sw = GetScreenWidth();
    int cX = sw / 2;
    int y = 180;
    int btnW = 260;
    int btnH = 44;
    int gap = 6;

    ClearBackground(Color{ 30, 33, 36, 255 });

    Font& font = game.GetFont();

    DrawGameText(font, "Settings", cX - MeasureGameText(font, "Settings", 40) / 2, 40, 40, WHITE);

    // Music toggle
    Rectangle rMusic = btnR((float)(cX - btnW / 2), (float)y, (float)btnW, (float)btnH);
    Color musicBg = m_musicOn ? ColorAlpha(GREEN, 0.4f) : ColorAlpha(RED, 0.3f);
    Color musicBorder = m_musicOn ? GREEN : RED;
    DrawRectangleRec(rMusic, musicBg);
    DrawRectangleLinesEx(rMusic, 1, musicBorder);
    DrawGameText(font, m_musicOn ? "Music: ON" : "Music: OFF",
                 cX - 50, y + 12, 18, WHITE);

    // Difficulty
    int diffY = y + btnH + 30;
    DrawGameText(font, "Default difficulty:", cX - 100, diffY - 22, 16, LIGHTGRAY);

    for (int i = 0; i < 4; ++i)
    {
        int by = diffY + i * (btnH + gap);
        bool sel = (i == m_difficultyPreset);
        Color bg = sel ? ColorAlpha(GREEN, 0.5f) : ColorAlpha(WHITE, 0.1f);
        Color border = sel ? GREEN : ColorAlpha(WHITE, 0.3f);
        Rectangle r = btnR((float)(cX - btnW / 2), (float)by, (float)btnW, (float)btnH);
        if (!sel && hov(r)) bg = ColorAlpha(WHITE, 0.2f);
        DrawRectangleRec(r, bg);
        DrawRectangleLinesEx(r, 1, border);
        DrawGameText(font, s_diffLabels[i],
                     cX - btnW / 2 + (sel ? 28 : 12), by + 12, 16,
                     sel ? WHITE : LIGHTGRAY);
        if (sel)
            DrawGameText(font, "✓", cX - btnW / 2 + 8, by + 10, 18, GREEN);
    }

    // Back
    int backY = diffY + 4 * (btnH + gap) + 20;
    Rectangle rBack = btnR((float)(cX - 100), (float)backY, 200.0f, 50.0f);
    DrawRectangleRec(rBack, ColorAlpha(ORANGE, 0.3f));
    DrawRectangleLinesEx(rBack, 1, ORANGE);
    DrawGameText(font, "Back", cX - 25, backY + 14, 18, WHITE);
}
