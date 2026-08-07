#include "states/MenuState.h"
#include "states/PlayState.h"
#include "states/SettingsState.h"
#include "states/MainMenuState.h"
#include "core/Game.h"
#include "core/DrawHelper.h"
#include "core/SaveManager.h"
#include "raylib.h"

#include <algorithm>
#include <cstdio>
#include <memory>

static const int ROW_H = 42;
static const int COL_W_NAME = 220;
static const int COL_W_TIME = 80;
static const int COL_W_PCT = 60;
static const int LIST_W = COL_W_NAME + COL_W_TIME + COL_W_PCT + 40;
static const int LIST_X = 60;
static const int LIST_Y = 120;
static const Color BG   = { 240, 242, 245, 255 };
static const Color CARD = { 255, 255, 255, 255 };
static const Color TEXT = { 40, 40, 45, 255 };
static const Color MUT  = { 150, 150, 155, 255 };
static const Color ACC  = { 55, 110, 210, 255 };
static const Color ACCL = { 220, 235, 255, 255 };
static const Color HOV  = { 235, 240, 250, 255 };

// Map preset index → rows/cols
static void GetDiffFromPreset(int preset, int& rows, int& cols)
{
    switch (preset) {
        case 0: rows=5; cols=5; break;
        case 1: rows=8; cols=8; break;
        case 2: rows=16; cols=16; break;
        case 3: rows=32; cols=32; break;
        default: rows=8; cols=8; break;
    }
}

void MenuState::Enter(Game&)
{
    m_selectedIndex = -1;
    m_scrollOffset = 0;
    m_preview = { 0 };
    m_lastPreviewIndex = -1;
}

void MenuState::Exit(Game&)
{
    if (m_preview.id != 0) { UnloadTexture(m_preview); m_preview = { 0 }; }
}

void MenuState::Update(Game& game)
{
    const auto& images = game.GetResources().GetImageList();
    int count = (int)images.size();
    Vector2 mouse = GetMousePosition();
    int sw = GetScreenWidth(), sh = GetScreenHeight();

    int preset = SettingsState::GetSettings().difficultyPreset;
    int tRows, tCols;
    GetDiffFromPreset(preset, tRows, tCols);

    // Row hover — only inside the list panel area (X must be checked too,
    // otherwise clicks on the Start button / preview select a row instead)
    m_hoveredIndex = -1;
    int listH = sh - LIST_Y - 30;
    bool inListX = (mouse.x >= LIST_X - 6 && mouse.x <= LIST_X + LIST_W + 6);
    bool inListY = (mouse.y >= LIST_Y - 6 && mouse.y <= LIST_Y - 6 + listH);
    if (inListX && inListY)
    {
        int relY = (int)mouse.y - LIST_Y + m_scrollOffset;
        int idx = relY / ROW_H;
        if (idx >= 0 && idx < count) m_hoveredIndex = idx;
    }

    // Click row to select and load preview
    if (inListX && inListY && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && m_hoveredIndex >= 0)
    {
        m_selectedIndex = m_hoveredIndex;
        if (m_lastPreviewIndex != m_selectedIndex)
        {
            if (m_preview.id != 0) UnloadTexture(m_preview);
            m_preview = { 0 };
            const auto& path = game.GetResources().GetImageList()[m_selectedIndex].filePath;
            Image img = LoadImage(path.c_str());
            if (img.data)
            {
                int ms = 300;
                if (img.width > ms || img.height > ms)
                { float s = (float)ms / std::max(img.width, img.height); ImageResize(&img, (int)(img.width*s), (int)(img.height*s)); }
                m_preview = LoadTextureFromImage(img);
                SetTextureFilter(m_preview, TEXTURE_FILTER_BILINEAR);
                UnloadImage(img);
            }
            m_lastPreviewIndex = m_selectedIndex;
        }
        // Don't start on single click — wait for Start button or Enter
        return;
    }

    // Enter key to start
    if (IsKeyPressed(KEY_ENTER) && m_selectedIndex >= 0)
    {
        game.selectedImageIndex = m_selectedIndex;
        game.selectedRows = tRows;
        game.selectedCols = tCols;
        game.continueGame = SaveManager::HasSave(m_selectedIndex, tRows, tCols);
        game.ChangeState(std::make_unique<PlayState>());
        return;
    }

    // Scroll
    int wheel = GetMouseWheelMove();
    if (wheel != 0)
    {
        m_scrollOffset -= wheel * ROW_H * 3;
        int maxScroll = std::max(0, count * ROW_H - GetScreenHeight() + LIST_Y + 40);
        m_scrollOffset = std::clamp(m_scrollOffset, 0, maxScroll);
    }

    // Start button — coordinates must match Draw() exactly
    int px = LIST_X + LIST_W + 40;
    int pz = 220;
    int btnX = px;
    int btnY = LIST_Y + 60 + pz + 20;
    Rectangle btnR = { (float)btnX, (float)btnY, (float)pz, 48 };
    if (CheckCollisionPointRec(mouse, btnR) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && m_selectedIndex >= 0)
    {
        game.selectedImageIndex = m_selectedIndex;
        game.selectedRows = tRows;
        game.selectedCols = tCols;
        game.continueGame = SaveManager::HasSave(m_selectedIndex, tRows, tCols);
        game.ChangeState(std::make_unique<PlayState>());
        return;
    }

    // Back
    if (mouse.x >= 15 && mouse.x < 85 && mouse.y >= 15 && mouse.y < 45 &&
        IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        game.ChangeState(std::make_unique<MainMenuState>());
}

void MenuState::Draw(Game& game)
{
    const auto& images = game.GetResources().GetImageList();
    int count = (int)images.size();
    int sh = GetScreenHeight(), sw = GetScreenWidth();
    Font& font = game.GetFont();

    DrawRectangle(0, 0, sw, sh, BG);

    int preset = SettingsState::GetSettings().difficultyPreset;
    int tRows, tCols;
    GetDiffFromPreset(preset, tRows, tCols);
    const char* diffNames[4] = { "25 pcs (5x5)", "64 pcs (8x8)", "256 pcs (16x16)", "1024 pcs (32x32)" };

    // Back
    DrawRectangle(12, 12, 75, 34, ColorAlpha(ACC, 0.10f));
    DrawRectangleLines(12, 12, 75, 34, ColorAlpha(ACC, 0.25f));
    DrawGameText(font, "\xE2\x86\x90 Back", 22, 20, 16, ACC);

    // Title
    DrawGameText(font, "Choose an image", LIST_X, 30, 26, TEXT);
    DrawGameText(font, TextFormat("%d images  |  %s", count, diffNames[preset]),
                 LIST_X, 62, 13, MUT);

    // List panel
    int listH = sh - LIST_Y - 30;
    DrawRectangle(LIST_X - 6, LIST_Y - 6, LIST_W + 12, listH, CARD);
    DrawRectangleLines(LIST_X - 6, LIST_Y - 6, LIST_W + 12, listH, ColorAlpha(MUT, 0.18f));

    // Column guides
    DrawLine(LIST_X + COL_W_NAME + 15, LIST_Y, LIST_X + COL_W_NAME + 15, LIST_Y + listH, ColorAlpha(MUT, 0.08f));
    DrawLine(LIST_X + COL_W_NAME + COL_W_TIME + 20, LIST_Y, LIST_X + COL_W_NAME + COL_W_TIME + 20, LIST_Y + listH, ColorAlpha(MUT, 0.08f));

    for (int i = 0; i < count; ++i)
    {
        int ry = LIST_Y + i * ROW_H - m_scrollOffset;
        if (ry + ROW_H < 0 || ry > sh) continue;

        bool sel = (i == m_selectedIndex);
        bool hov = (i == m_hoveredIndex);
        Color rowBg = CARD;
        if (sel) rowBg = ACCL;
        else if (hov) rowBg = HOV;
        DrawRectangle(LIST_X, ry, LIST_W, ROW_H, rowBg);
        if (sel) DrawRectangle(LIST_X + LIST_W - 4, ry + 4, 4, ROW_H - 8, ACC);
        if (!sel && !hov) DrawLine(LIST_X, ry + ROW_H, LIST_X + LIST_W, ry + ROW_H, ColorAlpha(MUT, 0.06f));

        // Col 1: filename
        DrawGameText(font, images[i].fileName.c_str(), LIST_X + 12, ry + 12, 14, TEXT);

        // Col 2: time
        float elapsed = 0;
        int pCount = 0;
        bool hs = SaveManager::LoadHeader(i, tRows, tCols, elapsed, pCount);
        if (hs)
        {
            char buf[16];
            snprintf(buf, sizeof(buf), "%02d:%02d", (int)(elapsed/60), (int)elapsed%60);
            DrawGameText(font, buf, LIST_X + COL_W_NAME + 18, ry + 12, 14, TEXT);
        }
        else
            DrawGameText(font, "\xE2\x80\x94", LIST_X + COL_W_NAME + 18, ry + 12, 14, MUT);

        // Col 3: progress
        if (hs) DrawGameText(font, "\xE2\x97\x8F", LIST_X + COL_W_NAME + COL_W_TIME + 22, ry + 11, 15, ACC);
        else    DrawGameText(font, "\xE2\x97\x8B", LIST_X + COL_W_NAME + COL_W_TIME + 22, ry + 11, 15, MUT);
    }

    // ---- Preview + Start button ----
    int px = LIST_X + LIST_W + 40;
    int py = LIST_Y + 60;
    int pz = 220;

    if (m_preview.id != 0)
    {
        DrawRectangleRounded({ (float)px-2, (float)py-2, (float)pz+4, (float)pz+4 }, 4, 1, CARD);
        DrawRectangleRoundedLines({ (float)px-2, (float)py-2, (float)pz+4, (float)pz+4 }, 4, 1, ColorAlpha(MUT, 0.18f));
        float sc = (float)pz / std::max((float)m_preview.width, (float)m_preview.height);
        float dw = m_preview.width*sc, dh = m_preview.height*sc;
        DrawTexturePro(m_preview, {0,0,(float)m_preview.width,(float)m_preview.height},
                       {(float)px+(pz-dw)/2,(float)py+(pz-dh)/2,dw,dh}, {0,0}, 0, WHITE);
    }
    else if (m_selectedIndex >= 0)
    {
        DrawRectangle(px, py, pz, pz, ColorAlpha(MUT, 0.06f));
        DrawGameText(font, "Preview", px+78, py+100, 14, MUT);
    }

    // Start button (below preview)
    int btnX = px, btnY = py + pz + 20;
    bool canStart = (m_selectedIndex >= 0);
    Vector2 mouse = GetMousePosition();
    Rectangle btnRR = { (float)btnX, (float)btnY, (float)pz, 48 };
    bool hoverBtn = CheckCollisionPointRec(mouse, btnRR) && canStart;
    Color btnBg = canStart ? (hoverBtn ? ColorAlpha(ACC, 0.15f) : ColorAlpha(ACC, 0.08f)) : ColorAlpha(MUT, 0.06f);
    Color btnBorder = canStart ? (hoverBtn ? ACC : ColorAlpha(ACC, 0.35f)) : ColorAlpha(MUT, 0.15f);
    Color btnText = canStart ? ACC : MUT;
    DrawRectangleRounded(btnRR, 6, 1, btnBg);
    DrawRectangleRoundedLines(btnRR, 6, 1, btnBorder);
    DrawGameText(font, "Start", btnX + pz/2 - 20, btnY + 15, 18, btnText);

    // Instructions
    DrawGameText(font, "Select an image  |  Press Start or Enter to play", 60, sh-28, 13, MUT);
}
