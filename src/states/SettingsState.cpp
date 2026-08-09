#include "states/SettingsState.h"
#include "states/MainMenuState.h"
#include "core/Game.h"
#include "core/Config.h"
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

static Rectangle btnR(float x, float y, float w, float h) { return Rectangle{ x, y, w, h }; }
static bool hov(Rectangle r) { return CheckCollisionPointRec(GetMousePosition(), r); }

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
    float s = Config::Instance().dpiScale * UserScale();
    int sw = GetScreenWidth();
    int cX = sw / 2;
    int y = (int)(180 * s);
    int btnW = (int)(260 * s);
    int btnH = (int)(44 * s);
    int gap = (int)(6 * s);

    Rectangle rMusic = btnR((float)(cX - btnW / 2), (float)y, (float)btnW, (float)btnH);
    int diffY = y + btnH + (int)(30 * s);
    int scaleY = diffY + 4 * (btnH + gap) + (int)(14 * s);

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

        // UI size: three radio buttons in a row
        int btnW3 = (int)((btnW - 2 * gap) / 3.0f);
        for (int i = 0; i < 3; ++i)
        {
            Rectangle r = btnR((float)(cX - btnW / 2 + i * (btnW3 + gap)),
                               (float)(scaleY + (int)(26 * s)), (float)btnW3, (float)(38 * s));
            if (hov(r)) { m_uiScale = i; break; }
        }

        // Back
        Rectangle rBack = btnR((float)(cX - 100 * s), (float)(scaleY + (int)(26 * s) + (int)(38 * s) + (int)(20 * s)),
                               200.0f * s, 50.0f * s);
        if (hov(rBack))
            game.ChangeState(std::make_unique<MainMenuState>());
    }
}

void SettingsState::Draw(Game& game)
{
    float s = Config::Instance().dpiScale * UserScale();
    int sw = GetScreenWidth();
    int cX = sw / 2;
    int y = (int)(180 * s);
    int btnW = (int)(260 * s);
    int btnH = (int)(44 * s);
    int gap = (int)(6 * s);

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

    int titleSize = (int)(40 * s);
    DrawGameText(font, "Настройки", cX - MeasureGameText(font, "Настройки", titleSize) / 2, 40, titleSize, TEXT);

    // Music toggle
    Rectangle rMusic = btnR((float)(cX - btnW / 2), (float)y, (float)btnW, (float)btnH);
    Color mBg = m_musicOn ? ACCL : CARD;
    Color mBorder = m_musicOn ? ACC : ColorAlpha(MUT, 0.4f);
    Color mText = m_musicOn ? ACC : MUT;
    DrawRectangleRounded(rMusic, 6, 1, mBg);
    DrawRectangleRoundedLines(rMusic, 6, 1, mBorder);
    int musicSize = (int)(18 * s);
    DrawGameText(font, m_musicOn ? "Музыка: вкл" : "Музыка: выкл",
                 cX - (int)(60 * s), y + (int)(12 * s), musicSize, mText);

    // Difficulty
    int diffY = y + btnH + (int)(30 * s);
    int labelSize = (int)(16 * s);
    DrawGameText(font, "Сложность по умолчанию:", cX - (int)(115 * s), diffY - (int)(22 * s), labelSize, MUT);

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
                     cX - btnW / 2 + (sel ? (int)(28 * s) : (int)(12 * s)), by + (int)(12 * s), labelSize,
                     textCol);
        if (sel)
            DrawGameText(font, "\xE2\x9C\x93", cX - btnW / 2 + (int)(8 * s), by + (int)(10 * s), (int)(18 * s), ACC);
    }

    // UI size
    int scaleY = diffY + 4 * (btnH + gap) + (int)(14 * s);
    DrawGameText(font, "Размер интерфейса:", cX - (int)(100 * s), scaleY - (int)(22 * s), labelSize, MUT);

    int btnW3 = (int)((btnW - 2 * gap) / 3.0f);
    for (int i = 0; i < 3; ++i)
    {
        int bx = cX - btnW / 2 + i * (btnW3 + gap);
        int by = scaleY + (int)(26 * s);
        bool sel = (i == m_uiScale);
        bool hv = hov(btnR((float)bx, (float)by, (float)btnW3, (float)(38 * s)));
        Color bg = sel ? ACCL : (hv ? HOV : CARD);
        Color border = sel ? ACC : ColorAlpha(MUT, 0.25f);
        Color textCol = sel ? ACC : TEXT;
        Rectangle r = btnR((float)bx, (float)by, (float)btnW3, (float)(38 * s));
        DrawRectangleRounded(r, 6, 1, bg);
        DrawRectangleRoundedLines(r, 6, 1, border);
        DrawGameText(font, s_scaleLabels[i],
                     bx + (int)((btnW3 - MeasureGameText(font, s_scaleLabels[i], (int)(15 * s))) / 2),
                     by + (int)(10 * s), (int)(15 * s), textCol);
    }

    // Back
    int backY = scaleY + (int)(26 * s) + (int)(38 * s) + (int)(20 * s);
    Rectangle rBack = btnR((float)(cX - 100 * s), (float)backY, 200.0f * s, 50.0f * s);
    bool hvBack = hov(rBack);
    DrawRectangleRounded(rBack, 6, 1, hvBack ? ColorAlpha(ACC, 0.08f) : CARD);
    DrawRectangleRoundedLines(rBack, 6, 1, hvBack ? ACC : ColorAlpha(ACC, 0.25f));
    int backSize = (int)(18 * s);
    DrawGameText(font, "Назад",
                 cX - (int)(MeasureGameText(font, "Назад", backSize) / 2 * s), backY + (int)(15 * s), backSize,
                 hvBack ? ACC : TEXT);
}
