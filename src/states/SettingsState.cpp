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

    // Shared light theme (same palette as MainMenu / MenuState)
    const Color BG   = { 240, 242, 245, 255 };
    const Color CARD = { 255, 255, 255, 255 };
    const Color TEXT = { 40, 40, 45, 255 };
    const Color MUT  = { 150, 150, 155, 255 };
    const Color ACC  = { 55, 110, 210, 255 };
    const Color ACCL = { 220, 235, 255, 255 };
    const Color HOV  = { 235, 240, 250, 255 };

    ClearBackground(BG);

    Font& font = game.GetFont();

    DrawGameText(font, "Settings", cX - MeasureGameText(font, "Settings", 40) / 2, 40, 40, TEXT);

    // Music toggle
    Rectangle rMusic = btnR((float)(cX - btnW / 2), (float)y, (float)btnW, (float)btnH);
    Color mBg = m_musicOn ? ACCL : CARD;
    Color mBorder = m_musicOn ? ACC : ColorAlpha(MUT, 0.4f);
    Color mText = m_musicOn ? ACC : MUT;
    DrawRectangleRounded(rMusic, 6, 1, mBg);
    DrawRectangleRoundedLines(rMusic, 6, 1, mBorder);
    DrawGameText(font, m_musicOn ? "Music: ON" : "Music: OFF",
                 cX - 55, y + 12, 18, mText);

    // Difficulty
    int diffY = y + btnH + 30;
    DrawGameText(font, "Default difficulty:", cX - 100, diffY - 22, 16, MUT);

    for (int i = 0; i < 4; ++i)
    {
        int by = diffY + i * (btnH + gap);
        bool sel = (i == m_difficultyPreset);
        bool hv = hov(btnR((float)(cX - btnW / 2), (float)by, (float)btnW, (float)btnH));
        Color bg = sel ? ACCL : (hv ? HOV : CARD);
        Color border = sel ? ACC : ColorAlpha(MUT, 0.25f);
        Color textCol = sel ? ACC : TEXT;
        Rectangle r = btnR((float)(cX - btnW / 2), (float)by, (float)btnW, (float)btnH);
        DrawRectangleRounded(r, 6, 1, bg);
        DrawRectangleRoundedLines(r, 6, 1, border);
        DrawGameText(font, s_diffLabels[i],
                     cX - btnW / 2 + (sel ? 28 : 12), by + 12, 16,
                     textCol);
        if (sel)
            DrawGameText(font, "✓", cX - btnW / 2 + 8, by + 10, 18, ACC);
    }

    // Back
    int backY = diffY + 4 * (btnH + gap) + 20;
    Rectangle rBack = btnR((float)(cX - 100), (float)backY, 200.0f, 50.0f);
    bool hvBack = hov(rBack);
    DrawRectangleRounded(rBack, 6, 1, hvBack ? ColorAlpha(ACC, 0.08f) : CARD);
    DrawRectangleRoundedLines(rBack, 6, 1, hvBack ? ACC : ColorAlpha(ACC, 0.25f));
    DrawGameText(font, "Back",
                 cX - MeasureGameText(font, "Back", 18) / 2, backY + 15, 18,
                 hvBack ? ACC : TEXT);
}
