#include "states/SettingsState.h"
#include "states/MainMenuState.h"
#include "core/Game.h"
#include "core/Config.h"
#include "core/DrawHelper.h"
#include "ui/Button.h"
#include "raylib.h"

#include <algorithm>
#include <cstdio>
#include <memory>

static SettingsData s_settings;

SettingsData& SettingsState::GetSettings() { return s_settings; }

void SettingsState::LoadFromFile()
{
    FILE* f = fopen("settings.dat", "rb");
    if (!f) return;
    size_t rd = fread(&s_settings, 1, sizeof(s_settings), f);
    fclose(f);
    // Old format files are shorter — the trailing uiScale field stays default
    if (rd < sizeof(s_settings))
        s_settings.uiScale = 1;
}

void SettingsState::SaveToFile()
{
    FILE* f = fopen("settings.dat", "wb");
    if (!f) return;
    fwrite(&s_settings, sizeof(s_settings), 1, f);
    fclose(f);
}

float SettingsState::UserScale()
{
    switch (s_settings.uiScale)
    {
        case 0: return 0.8f;   // маленький
        case 2: return 1.25f;  // большой
        default: return 1.0f;  // нормальный
    }
}

static const char* s_diffLabels[4] = {
    "25 шт (5x5)",
    "64 шт (8x8)",
    "256 шт (16x16)",
    "1024 шт (32x32)",
};

static const char* s_scaleLabels[3] = {
    "Маленький",
    "Нормальный",
    "Большой",
};

void SettingsState::Enter(Game&)
{
    m_musicOn = s_settings.musicOn;
    m_difficultyPreset = s_settings.difficultyPreset;
    m_uiScale = s_settings.uiScale;
}

void SettingsState::Exit(Game&)
{
    s_settings.musicOn = m_musicOn;
    s_settings.difficultyPreset = m_difficultyPreset;
    s_settings.uiScale = m_uiScale;
    SaveToFile();
}

void SettingsState::Update(Game& game)
{
    const float y = 180, gap = 6;
    Font& font = game.GetFont();

    // Build buttons (auto-sized), compute their rects, then handle clicks
    ui::Button bMusic(0, 0, 0, 0, m_musicOn ? "Музыка: вкл" : "Музыка: выкл");
    bMusic.SetFontSize(18);

    ui::Button bDiffs[4];
    for (int i = 0; i < 4; ++i)
    {
        bDiffs[i].SetLabel(s_diffLabels[i]);
        bDiffs[i].SetFontSize(16);
    }

    ui::Button bScales[3];
    for (int i = 0; i < 3; ++i)
    {
        bScales[i].SetLabel(s_scaleLabels[i]);
        bScales[i].SetFontSize(15);
    }

    ui::Button bBack(0, 0, 0, 0, "Назад");
    bBack.SetFontSize(18);

    bMusic.UpdateAutoSize(font);
    for (int i = 0; i < 4; ++i) bDiffs[i].UpdateAutoSize(font);
    for (int i = 0; i < 3; ++i) bScales[i].UpdateAutoSize(font);
    bBack.UpdateAutoSize(font);

    float w = 0, h = 0;
    for (auto* b : { &bMusic, &bDiffs[0], &bDiffs[1], &bDiffs[2], &bDiffs[3],
                     &bScales[0], &bScales[1], &bScales[2], &bBack })
    {
        w = std::max(w, b->MinWidth());
        h = std::max(h, b->MinHeight());
    }

    const float cx = ui::LogW() / 2;
    const float diffY = y + h + 30;
    const float scaleY = diffY + 4 * (h + gap) + 14;
    const float bx = cx - w / 2;

    bMusic.SetRect(bx, y, w, 0);
    for (int i = 0; i < 4; ++i)
        bDiffs[i].SetRect(bx, diffY + i * (h + gap), w, 0);

    // UI size: three radio buttons in a row, equal width
    float w3 = 0;
    for (int i = 0; i < 3; ++i) w3 = std::max(w3, bScales[i].MinWidth());
    for (int i = 0; i < 3; ++i)
        bScales[i].SetRect(bx + i * (w3 + gap), scaleY + 26, w3, 0);

    bBack.SetRect(bx, scaleY + 26 + h + 20, w, 0);

    // Music toggle
    if (bMusic.Clicked())
        m_musicOn = !m_musicOn;

    // Difficulty
    for (int i = 0; i < 4; ++i)
    {
        if (bDiffs[i].Clicked()) { m_difficultyPreset = i; break; }
    }

    // UI size
    for (int i = 0; i < 3; ++i)
    {
        if (bScales[i].Clicked()) { m_uiScale = i; break; }
    }

    // Back
    if (bBack.Clicked())
        game.ChangeState(std::make_unique<MainMenuState>());
}

void SettingsState::Draw(Game& game)
{
    float s = ui::UiScale();

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

    const float y = 180, gap = 6;

    // Build buttons (same as Update) and lay them out
    ui::Button bMusic(0, 0, 0, 0, m_musicOn ? "вкл" : "выкл");
    bMusic.SetFontSize(18);

    ui::Button bDiffs[4];
    for (int i = 0; i < 4; ++i) { bDiffs[i].SetLabel(s_diffLabels[i]); bDiffs[i].SetFontSize(16); }

    ui::Button bScales[3];
    for (int i = 0; i < 3; ++i) { bScales[i].SetLabel(s_scaleLabels[i]); bScales[i].SetFontSize(15); }

    ui::Button bBack(0, 0, 0, 0, "Назад");
    bBack.SetFontSize(18);

    bMusic.UpdateAutoSize(font);
    for (int i = 0; i < 4; ++i) bDiffs[i].UpdateAutoSize(font);
    for (int i = 0; i < 3; ++i) bScales[i].UpdateAutoSize(font);
    bBack.UpdateAutoSize(font);

    float w = 0, h = 0;
    for (auto* b : { &bMusic, &bDiffs[0], &bDiffs[1], &bDiffs[2], &bDiffs[3],
                     &bScales[0], &bScales[1], &bScales[2], &bBack })
    {
        w = std::max(w, b->MinWidth());
        h = std::max(h, b->MinHeight());
    }

    const float cx = ui::LogW() / 2;
    const float diffY = y + h + 30;
    const float scaleY = diffY + 4 * (h + gap) + 14;
    const float bx = cx - w / 2;

    bMusic.SetRect(bx, y, w, 0);
    for (int i = 0; i < 4; ++i)
        bDiffs[i].SetRect(bx, diffY + i * (h + gap), w, 0);
    float w3 = 0;
    for (int i = 0; i < 3; ++i) w3 = std::max(w3, bScales[i].MinWidth());
    for (int i = 0; i < 3; ++i)
        bScales[i].SetRect(bx + i * (w3 + gap), scaleY + 26, w3, 0);
    bBack.SetRect(bx, scaleY + 26 + h + 20, w, 0);

    int titleSize = (int)(40 * s);
    DrawGameText(font, "Настройки",
                 (int)ui::LogW() / 2 - MeasureGameText(font, "Настройки", titleSize) / 2,
                 40, titleSize, TEXT);

    // Music toggle
    bMusic.SetColors(m_musicOn ? ACCL : CARD, m_musicOn ? ACC : ColorAlpha(MUT, 0.4f),
                     m_musicOn ? ACC : MUT);
    bMusic.SetHoverColors(m_musicOn ? ACCL : HOV, m_musicOn ? ACC : ColorAlpha(MUT, 0.5f),
                          m_musicOn ? ACC : MUT);
    bMusic.Draw(font);

    // Difficulty
    int labelSize = (int)(16 * s);
    DrawGameText(font,
                 "Сложность по умолчанию:",
                 (int) bx - (int) (125 * s),
                 (int) diffY - (int) (10 * s),
                 labelSize,
                 MUT);

    for (int i = 0; i < 4; ++i)
    {
        bool sel = (i == m_difficultyPreset);
        bDiffs[i].SetColors(sel ? ACCL : CARD, sel ? ACC : ColorAlpha(MUT, 0.25f),
                            sel ? ACC : TEXT);
        bDiffs[i].SetHoverColors(sel ? ACCL : HOV, sel ? ACC : ColorAlpha(ACC, 0.5f),
                                 sel ? ACC : TEXT);
        bDiffs[i].Draw(font);

        if (sel)
        {
            DrawGameText(font, "\xE2\x9C\x93",
                         (int)bx + (int)(8 * s), (int)(diffY + i * (h + gap) + 10 * s),
                         (int)(18 * s), ACC);
        }
    }

    // UI size
    DrawGameText(font, "Размер интерфейса:", (int)bx + (int)(20 * s), (int)scaleY - (int)(22 * s),
                 labelSize, MUT);

    for (int i = 0; i < 3; ++i)
    {
        bool sel = (i == m_uiScale);
        bScales[i].SetColors(sel ? ACCL : CARD, sel ? ACC : ColorAlpha(MUT, 0.25f),
                             sel ? ACC : TEXT);
        bScales[i].SetHoverColors(sel ? ACCL : HOV, sel ? ACC : ColorAlpha(ACC, 0.5f),
                                  sel ? ACC : TEXT);
        bScales[i].Draw(font);
    }

    // Back
    bBack.SetColors(CARD, ColorAlpha(ACC, 0.25f), TEXT);
    bBack.SetHoverColors(ColorAlpha(ACC, 0.08f), ACC, ACC);
    bBack.Draw(font);
}
