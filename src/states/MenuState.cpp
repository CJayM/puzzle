#include "states/MenuState.h"
#include "states/PlayState.h"
#include "states/SettingsState.h"
#include "core/Game.h"
#include "core/DrawHelper.h"
#include "core/SaveManager.h"
#include "raylib.h"

#include <algorithm>
#include <memory>

const DifficultyPreset MenuState::s_presets[4] = {
    { "25 pcs  (5x5)",     5,  5,  25   },
    { "64 pcs  (8x8)",     8,  8,  64   },
    { "256 pcs  (16x16)",  16, 16, 256  },
    { "1024 pcs  (32x32)", 32, 32, 1024 },
};

static const int ITEM_HEIGHT = 40;
static const int ITEM_PAD_X = 20;
static const int LIST_X = 60;
static const int LIST_Y = 120;
static const int LIST_WIDTH = 400;
static const int BTN_WIDTH = 140;
static const int BTN_HEIGHT = 44;

void MenuState::Enter(Game&)
{
    m_selectedIndex = -1;
    m_scrollOffset = 0;
    m_selectedPreset = 2; // default 8x8 = 64
    m_preview = { 0 };
    m_lastPreviewIndex = -1;
}

void MenuState::Exit(Game&)
{
    if (m_preview.id != 0)
    {
        UnloadTexture(m_preview);
        m_preview = { 0 };
    }
}

void MenuState::Update(Game& game)
{
    const auto& images = game.GetResources().GetImageList();
    int count = static_cast<int>(images.size());

    Vector2 mouse = GetMousePosition();
    int sw = GetScreenWidth();

    // Image list hover
    m_hoveredIndex = -1;
    if (mouse.x >= LIST_X && mouse.x < LIST_X + LIST_WIDTH)
    {
        int relY = static_cast<int>(mouse.y) - LIST_Y + m_scrollOffset;
        int idx = relY / ITEM_HEIGHT;
        if (idx >= 0 && idx < count)
            m_hoveredIndex = idx;
    }

    // Click to select image
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && m_hoveredIndex >= 0)
    {
        m_selectedIndex = m_hoveredIndex;

        // Load preview directly (not via ResourceManager cache)
        if (m_lastPreviewIndex != m_selectedIndex)
        {
            if (m_preview.id != 0) UnloadTexture(m_preview);
            m_preview = { 0 };

            const auto& path = game.GetResources().GetImageList()[m_selectedIndex].filePath;
            Image img = LoadImage(path.c_str());
            if (img.data != nullptr)
            {
                int maxSize = 300;
                if (img.width > maxSize || img.height > maxSize)
                {
                    float scale = (float)maxSize / std::max(img.width, img.height);
                    ImageResize(&img, (int)(img.width * scale), (int)(img.height * scale));
                }
                m_preview = LoadTextureFromImage(img);
                SetTextureFilter(m_preview, TEXTURE_FILTER_BILINEAR);
                UnloadImage(img);
            }
            m_lastPreviewIndex = m_selectedIndex;
        }
    }

    // Scroll
    int wheel = GetMouseWheelMove();
    if (wheel != 0)
    {
        m_scrollOffset -= wheel * ITEM_HEIGHT * 3;
        int maxScroll = std::max(0, count * ITEM_HEIGHT - GetScreenHeight() + LIST_Y + 40);
        m_scrollOffset = std::clamp(m_scrollOffset, 0, maxScroll);
    }

    // Difficulty toggles (right of image list)
    int toggleX = LIST_X + LIST_WIDTH + 50;
    int toggleY = 120 + 220 + 20; // previewY + previewSize + gap
    int toggleW = 190;
    int toggleH = 44;
    int gap = 6;

    for (int i = 0; i < 4; ++i)
    {
        Rectangle r = { (float)toggleX, (float)(toggleY + i * (toggleH + gap)),
                        (float)toggleW, (float)toggleH };
        if (CheckCollisionPointRec(mouse, r) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            m_selectedPreset = i;
    }

    // "Start" button
    int btnX = LIST_X + LIST_WIDTH - BTN_WIDTH;
    int btnY = GetScreenHeight() - 60;
    bool hoverBtn = (mouse.x >= btnX && mouse.x < btnX + BTN_WIDTH &&
                     mouse.y >= btnY && mouse.y < btnY + BTN_HEIGHT);
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && hoverBtn && m_selectedIndex >= 0)
    {
        auto& p = s_presets[m_selectedPreset];
        game.selectedImageIndex = m_selectedIndex;
        game.selectedRows = p.rows;
        game.selectedCols = p.cols;
        game.continueGame = SaveManager::HasSave(m_selectedIndex, p.rows, p.cols);
        game.ChangeState(std::make_unique<PlayState>());
    }

    // "Settings" link
    if (mouse.x >= sw - 100 && mouse.x < sw - 10 &&
        mouse.y >= 10 && mouse.y < 35)
    {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            game.ChangeState(std::make_unique<SettingsState>());
    }
}

void MenuState::Draw(Game& game)
{
    const auto& images = game.GetResources().GetImageList();
    int count = static_cast<int>(images.size());
    int screenH = GetScreenHeight();
    int sw = GetScreenWidth();

    ClearBackground(Color{ 30, 33, 36, 255 });

    // Title
    DrawGameText(game.GetFont(), "Jigsaw Puzzle", LIST_X, 20, 40, WHITE);
    DrawGameText(game.GetFont(), "Choose an image:", LIST_X, 70, 20, LIGHTGRAY);

    if (count == 0)
    {
        DrawGameText(game.GetFont(), "No images found in assets/images/", LIST_X, LIST_Y, 20, YELLOW);
        DrawGameText(game.GetFont(), "Add .png / .jpg / .bmp files and restart.", LIST_X, LIST_Y + 30, 20, YELLOW);
        return;
    }

    // Background for image list
    int listVisualHeight = screenH - LIST_Y - 80;
    DrawRectangle(LIST_X - 5, LIST_Y - 5, LIST_WIDTH + 10,
                  listVisualHeight, ColorAlpha(BLACK, 0.3f));

    // Draw each image item
    for (int i = 0; i < count; ++i)
    {
        int itemY = LIST_Y + i * ITEM_HEIGHT - m_scrollOffset;
        if (itemY + ITEM_HEIGHT < 0 || itemY > screenH) continue;

        Color bg = BLANK;
        if (i == m_selectedIndex)
            bg = ColorAlpha(SKYBLUE, 0.5f);
        else if (i == m_hoveredIndex)
            bg = ColorAlpha(WHITE, 0.15f);

        if (bg.a > 0)
            DrawRectangle(LIST_X, itemY, LIST_WIDTH, ITEM_HEIGHT, bg);

        DrawGameText(game.GetFont(), images[i].fileName.c_str(),
                 LIST_X + ITEM_PAD_X, itemY + 8, 20, WHITE);
    }

    // ---- Preview ----
    int previewX = LIST_X + LIST_WIDTH + 50;
    int previewY = 120;
    int previewSize = 220;

    if (m_preview.id != 0)
    {
        DrawRectangle(previewX - 2, previewY - 2, previewSize + 4, previewSize + 4,
                      ColorAlpha(WHITE, 0.15f));
        float scale = (float)previewSize / std::max((float)m_preview.width, (float)m_preview.height);
        float drawW = m_preview.width * scale;
        float drawH = m_preview.height * scale;
        DrawTexturePro(m_preview,
            Rectangle{ 0, 0, (float)m_preview.width, (float)m_preview.height },
            Rectangle{ (float)previewX + (previewSize - drawW) / 2.0f,
                       (float)previewY + (previewSize - drawH) / 2.0f,
                       drawW, drawH },
            Vector2{ 0, 0 }, 0.0f, WHITE);
    }
    else if (m_selectedIndex >= 0)
    {
        DrawRectangle(previewX, previewY, previewSize, previewSize,
                      ColorAlpha(GRAY, 0.3f));
        DrawGameText(game.GetFont(), "Loading...", previewX + 60, previewY + 100, 16, GRAY);
    }

    // ---- Difficulty toggles ----
    int toggleX = LIST_X + LIST_WIDTH + 50;
    int toggleY = previewY + previewSize + 20;
    int toggleW = 190;
    int toggleH = 44;
    int gap = 6;

    Vector2 mouse = GetMousePosition();
    for (int i = 0; i < 4; ++i)
    {
        int by = toggleY + i * (toggleH + gap);
        bool selected = (i == m_selectedPreset);

        Color bg = selected ? ColorAlpha(GREEN, 0.5f) : ColorAlpha(WHITE, 0.1f);
        Color border = selected ? GREEN : ColorAlpha(WHITE, 0.3f);

        Rectangle r = { (float)toggleX, (float)by, (float)toggleW, (float)toggleH };
        bool hov = CheckCollisionPointRec(mouse, r);
        if (!selected && hov) bg = ColorAlpha(WHITE, 0.2f);

        DrawRectangleRec(r, bg);
        DrawRectangleLinesEx(r, 1, border);

        DrawGameText(game.GetFont(), s_presets[i].label,
                 toggleX + (selected ? 22 : 10), by + 12, 16,
                 selected ? WHITE : LIGHTGRAY);
        if (selected)
            DrawGameText(game.GetFont(), "✓", toggleX + 6, by + 10, 18, GREEN);
    }

    // ---- "Start" button ----
    int btnX = LIST_X + LIST_WIDTH - BTN_WIDTH;
    int btnY = screenH - 60;
    bool canStart = (m_selectedIndex >= 0);

    Color btnColor = canStart ? ColorAlpha(SKYBLUE, 0.8f) : ColorAlpha(GRAY, 0.3f);
    bool hoverBtn = (mouse.x >= btnX && mouse.x < btnX + BTN_WIDTH &&
                     mouse.y >= btnY && mouse.y < btnY + BTN_HEIGHT);
    if (canStart && hoverBtn) btnColor = SKYBLUE;

    DrawRectangle(btnX, btnY, BTN_WIDTH, BTN_HEIGHT, btnColor);
    DrawRectangleLines(btnX, btnY, BTN_WIDTH, BTN_HEIGHT,
                       canStart ? WHITE : ColorAlpha(WHITE, 0.3f));

    const char* startLabel = "Start";
    if (m_selectedIndex >= 0)
    {
        auto& p = s_presets[m_selectedPreset];
        startLabel = TextFormat("Start (%d pcs)", p.total);
    }
    DrawGameText(game.GetFont(), startLabel, btnX + 12, btnY + 12, 18, canStart ? WHITE : GRAY);

    // Info line
    if (m_selectedIndex >= 0 && m_selectedIndex < count)
    {
        DrawGameText(game.GetFont(), TextFormat("Selected: %s", images[m_selectedIndex].fileName.c_str()),
                 LIST_X, screenH - 90, 16, SKYBLUE);
    }

    DrawGameText(game.GetFont(), "Click to select  |  Choose difficulty  |  Then Start",
             10, screenH - 30, 14, GRAY);

    // Settings link
    DrawGameText(game.GetFont(), "Settings", sw - 95, 12, 18, ColorAlpha(SKYBLUE, 0.7f));
}
