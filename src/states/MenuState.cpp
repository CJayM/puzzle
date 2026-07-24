#include "states/MenuState.h"
#include "states/DifficultyState.h"
#include "core/Game.h"
#include "raylib.h"

#include <algorithm>
#include <memory>

static const int ITEM_HEIGHT = 40;
static const int ITEM_PAD_X = 20;
static const int LIST_X = 60;
static const int LIST_Y = 120;
static const int LIST_WIDTH = 400;
static const int BTN_WIDTH = 120;
static const int BTN_HEIGHT = 40;

void MenuState::Enter(Game&)
{
    m_selectedIndex = -1;
    m_scrollOffset = 0;
}

void MenuState::Exit(Game&)
{
}

void MenuState::Update(Game& game)
{
    const auto& images = game.GetResources().GetImageList();
    int count = static_cast<int>(images.size());

    Vector2 mouse = GetMousePosition();

    // Compute which item the mouse is hovering over
    m_hoveredIndex = -1;
    if (mouse.x >= LIST_X && mouse.x < LIST_X + LIST_WIDTH)
    {
        int relY = static_cast<int>(mouse.y) - LIST_Y + m_scrollOffset;
        int idx = relY / ITEM_HEIGHT;
        if (idx >= 0 && idx < count)
            m_hoveredIndex = idx;
    }

    // Click to select
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && m_hoveredIndex >= 0)
    {
        m_selectedIndex = m_hoveredIndex;
    }

    // Scroll wheel
    int wheel = GetMouseWheelMove();
    if (wheel != 0)
    {
        m_scrollOffset -= wheel * ITEM_HEIGHT * 3;
        int maxScroll = std::max(0, count * ITEM_HEIGHT - GetScreenHeight() + LIST_Y + 40);
        m_scrollOffset = std::clamp(m_scrollOffset, 0, maxScroll);
    }

    // "Next >" button
    int btnX = LIST_X + LIST_WIDTH - BTN_WIDTH;
    int btnY = GetScreenHeight() - 60;
    bool hoverBtn = (mouse.x >= btnX && mouse.x < btnX + BTN_WIDTH &&
                     mouse.y >= btnY && mouse.y < btnY + BTN_HEIGHT);
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && hoverBtn && m_selectedIndex >= 0)
    {
        game.selectedImageIndex = m_selectedIndex;
        game.ChangeState(std::make_unique<DifficultyState>());
    }
}

void MenuState::Draw(Game& game)
{
    const auto& images = game.GetResources().GetImageList();
    int count = static_cast<int>(images.size());

    ClearBackground(Color{ 30, 33, 36, 255 }); // dark gray

    // Title
    DrawText("Jigsaw Puzzle", LIST_X, 20, 40, WHITE);
    DrawText("Choose an image:", LIST_X, 70, 20, LIGHTGRAY);

    if (count == 0)
    {
        DrawText("No images found in assets/images/", LIST_X, LIST_Y, 20, YELLOW);
        DrawText("Add .png / .jpg / .bmp files and restart.", LIST_X, LIST_Y + 30, 20, YELLOW);
        return;
    }

    // Background for list area
    int screenH = GetScreenHeight();
    int listVisualHeight = screenH - LIST_Y - 80;
    DrawRectangle(LIST_X - 5, LIST_Y - 5, LIST_WIDTH + 10,
                  listVisualHeight, ColorAlpha(BLACK, 0.3f));

    // Draw each visible item
    for (int i = 0; i < count; ++i)
    {
        int itemY = LIST_Y + i * ITEM_HEIGHT - m_scrollOffset;

        if (itemY + ITEM_HEIGHT < 0 || itemY > screenH)
            continue;

        Color bg = BLANK;
        if (i == m_selectedIndex)
            bg = ColorAlpha(SKYBLUE, 0.5f);
        else if (i == m_hoveredIndex)
            bg = ColorAlpha(WHITE, 0.15f);

        if (bg.a > 0)
            DrawRectangle(LIST_X, itemY, LIST_WIDTH, ITEM_HEIGHT, bg);

        DrawText(images[i].fileName.c_str(),
                 LIST_X + ITEM_PAD_X, itemY + 8, 20, WHITE);
    }

    // "Next >" button
    int btnX = LIST_X + LIST_WIDTH - BTN_WIDTH;
    int btnY = screenH - 60;
    bool canProceed = (m_selectedIndex >= 0);
    Color btnColor = canProceed ? ColorAlpha(SKYBLUE, 0.8f) : ColorAlpha(GRAY, 0.3f);

    Vector2 mouse = GetMousePosition();
    bool hoverBtn = (mouse.x >= btnX && mouse.x < btnX + BTN_WIDTH &&
                     mouse.y >= btnY && mouse.y < btnY + BTN_HEIGHT);

    if (canProceed && hoverBtn)
        btnColor = SKYBLUE;

    DrawRectangle(btnX, btnY, BTN_WIDTH, BTN_HEIGHT, btnColor);
    DrawRectangleLines(btnX, btnY, BTN_WIDTH, BTN_HEIGHT,
                       canProceed ? WHITE : ColorAlpha(WHITE, 0.3f));
    DrawText("Next >", btnX + 20, btnY + 10, 20, canProceed ? WHITE : GRAY);

    // Selected info
    if (m_selectedIndex >= 0 && m_selectedIndex < count)
    {
        DrawText(TextFormat("Selected: %s", images[m_selectedIndex].fileName.c_str()),
                 LIST_X, screenH - 90, 16, SKYBLUE);
    }

    // Instructions
    DrawText("Click to select  |  Scroll to browse  |  Then click Next",
             10, screenH - 30, 14, GRAY);
}
