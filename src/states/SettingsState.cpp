#include "states/SettingsState.h"
#include "states/MainMenuState.h"
#include "core/Game.h"
#include "core/Config.h"
#include "core/DrawHelper.h"
#include "ui/Button.h"
#include "ui/Label.h"
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

// ---- UI construction: widgets created once, laid out via nested layouts ----

static void StyleSelectable(ui::Button& b, bool sel, Color ACCL, Color CARD,
                            Color ACC, Color MUT, Color TEXT, Color HOV)
{
    b.SetColors(sel ? ACCL : CARD, sel ? ACC : ColorAlpha(MUT, 0.25f),
                sel ? ACC : TEXT);
    b.SetHoverColors(sel ? ACCL : HOV, sel ? ACC : ColorAlpha(ACC, 0.5f),
                     sel ? ACC : TEXT);
}

void SettingsState::InitUi()
{
    const Color MUT = { 150, 150, 155, 255 };

    // Labels
    m_lblSound.SetText("Звуки:");
    m_lblSound.SetFontSize(18);
    m_lblSound.SetColor({ 40, 40, 45, 255 });
    m_lblDiff.SetText("Сложность по умолчанию:");
    m_lblDiff.SetFontSize(16);
    m_lblDiff.SetColor(MUT);
    m_lblScale.SetText("Размер интерфейса:");
    m_lblScale.SetFontSize(16);
    m_lblScale.SetColor(MUT);

    // Music button
    m_btnMusic.SetFontSize(18);

    // Difficulty buttons
    for (int i = 0; i < 4; ++i)
    {
        m_btnDiffs[i].SetLabel(s_diffLabels[i]);
        m_btnDiffs[i].SetFontSize(16);
    }

    // Scale radio buttons
    for (int i = 0; i < 3; ++i)
    {
        m_btnScales[i].SetLabel(s_scaleLabels[i]);
        m_btnScales[i].SetFontSize(15);
    }

    // Back button
    m_btnBack.SetLabel("Назад");
    m_btnBack.SetFontSize(18);
    m_btnBack.SetColors(Color{ 255, 255, 255, 255 }, ColorAlpha({ 55, 110, 210, 255 }, 0.25f),
                        Color{ 40, 40, 45, 255 });
    m_btnBack.SetHoverColors(ColorAlpha({ 55, 110, 210, 255 }, 0.08f),
                             Color{ 55, 110, 210, 255 }, Color{ 55, 110, 210, 255 });

    // Layouts: rows of [label, control], nested inside the main VLayout
    m_rowSound.SetSpacing(16);
    m_rowSound.AddItem(&m_lblSound);
    m_rowSound.AddItem(&m_btnMusic);

    m_diffCol.SetSpacing(6);
    for (int i = 0; i < 4; ++i)
        m_diffCol.AddItem(&m_btnDiffs[i]);
    m_rowDiff.SetSpacing(16);
    m_rowDiff.AddItem(&m_lblDiff);
    m_rowDiff.AddItem(&m_diffCol);

    m_scaleRow.SetSpacing(6);
    for (int i = 0; i < 3; ++i)
        m_scaleRow.AddItem(&m_btnScales[i]);
    m_rowScale.SetSpacing(16);
    m_rowScale.AddItem(&m_lblScale);
    m_rowScale.AddItem(&m_scaleRow);

    m_mainLayout.SetSpacing(16);
    m_mainLayout.AddItem(&m_rowSound);
    m_mainLayout.AddItem(&m_rowDiff);
    m_mainLayout.AddItem(&m_rowScale);
    m_mainLayout.AddItem(&m_btnBack);
}

// Refresh auto sizes, widget styles and layout positions
void SettingsState::UpdateLayout(Font& font)
{
    const Color ACC  = { 55, 110, 210, 255 };
    const Color CARD = { 255, 255, 255, 255 };
    const Color TEXT = { 40, 40, 45, 255 };
    const Color MUT  = { 150, 150, 155, 255 };
    const Color ACCL = { 220, 235, 255, 255 };
    const Color HOV  = { 235, 240, 250, 255 };

    // Music state label + style
    m_btnMusic.SetLabel(m_musicOn ? "Музыка: вкл" : "Музыка: выкл");
    m_btnMusic.SetColors(m_musicOn ? ACCL : CARD, m_musicOn ? ACC : ColorAlpha(MUT, 0.4f),
                         m_musicOn ? ACC : MUT);
    m_btnMusic.SetHoverColors(m_musicOn ? ACCL : HOV, m_musicOn ? ACC : ColorAlpha(MUT, 0.5f),
                              m_musicOn ? ACC : MUT);

    // Auto sizes
    m_lblSound.UpdateAutoSize(font);
    m_lblDiff.UpdateAutoSize(font);
    m_lblScale.UpdateAutoSize(font);
    m_btnMusic.UpdateAutoSize(font);
    for (int i = 0; i < 4; ++i) m_btnDiffs[i].UpdateAutoSize(font);
    for (int i = 0; i < 3; ++i) m_btnScales[i].UpdateAutoSize(font);
    m_btnBack.UpdateAutoSize(font);

    // Selection styles (diffs, scales)
    for (int i = 0; i < 4; ++i)
        StyleSelectable(m_btnDiffs[i], i == m_difficultyPreset, ACCL, CARD, ACC, MUT, TEXT, HOV);
    for (int i = 0; i < 3; ++i)
        StyleSelectable(m_btnScales[i], i == m_uiScale, ACCL, CARD, ACC, MUT, TEXT, HOV);

    // Centered main column
    float w = m_mainLayout.MinWidth();
    m_mainLayout.Compute(ui::LogW() / 2 - w / 2, 170, 0, 0);
}

void SettingsState::Enter(Game&)
{
    m_musicOn = s_settings.musicOn;
    m_difficultyPreset = s_settings.difficultyPreset;
    m_uiScale = s_settings.uiScale;

    InitUi();
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
    UpdateLayout(game.GetFont());

    // Music toggle
    if (m_btnMusic.Clicked())
        m_musicOn = !m_musicOn;

    // Difficulty
    for (int i = 0; i < 4; ++i)
    {
        if (m_btnDiffs[i].Clicked()) { m_difficultyPreset = i; break; }
    }

    // UI size
    for (int i = 0; i < 3; ++i)
    {
        if (m_btnScales[i].Clicked()) { m_uiScale = i; break; }
    }

    // Back
    if (m_btnBack.Clicked())
        game.ChangeState(std::make_unique<MainMenuState>());
}

void SettingsState::Draw(Game& game)
{
    float s = ui::UiScale();

    const Color BG   = { 240, 242, 245, 255 };
    const Color ACC  = { 55, 110, 210, 255 };

    ClearBackground(BG);

    Font& font = game.GetFont();

    UpdateLayout(font);

    int titleSize = (int)(40 * s);
    DrawGameText(font, "Настройки",
                 (int)ui::LogW() / 2 - MeasureGameText(font, "Настройки", titleSize) / 2,
                 40, titleSize, Color{ 40, 40, 45, 255 });

    // Draw the whole nested layout (labels + buttons)
    m_mainLayout.Draw(font);

    // Check marks for the selected difficulty (on top of the buttons)
    for (int i = 0; i < 4; ++i)
    {
        if (i == m_difficultyPreset)
        {
            Rectangle r = m_btnDiffs[i].Rect();
            DrawGameText(font, "\xE2\x9C\x93",
                         (int)r.x + (int)(8 * s), (int)r.y + (int)(10 * s),
                         (int)(18 * s), ACC);
        }
    }
}
