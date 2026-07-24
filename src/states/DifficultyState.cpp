#include "states/DifficultyState.h"
#include "core/Game.h"
#include "raylib.h"

#include <memory>

const DifficultyOption DifficultyState::s_options[6] = {
    { "3 x 3  (9 pieces)",   3, 3, 9  },
    { "4 x 4  (16 pieces)",  4, 4, 16 },
    { "5 x 5  (25 pieces)",  5, 5, 25 },
    { "6 x 6  (36 pieces)",  6, 6, 36 },
    { "7 x 7  (49 pieces)",  7, 7, 49 },
    { "8 x 8  (64 pieces)",  8, 8, 64 },
};

static const int OPT_BTN_W = 260;
static const int OPT_BTN_H = 50;
static const int OPT_START_X = 500;
static const int OPT_START_Y = 120;
static const int OPT_GAP = 10;
static const int PREVIEW_SIZE = 300;

void DifficultyState::Enter(Game& game)
{
    m_selectedOption = 2; // default 4x4

    // Load preview thumbnail
    if (game.selectedImageIndex >= 0)
    {
        m_preview = game.GetResources().LoadThumbnail(game.selectedImageIndex, PREVIEW_SIZE);
    }
}

void DifficultyState::Exit(Game&)
{
    if (m_preview.id != 0)
    {
        UnloadTexture(m_preview);
        m_preview = { 0 };
    }
}

void DifficultyState::Update(Game& game)
{
    Vector2 mouse = GetMousePosition();

    // Check option button clicks
    for (int i = 0; i < 6; ++i)
    {
        int btnY = OPT_START_Y + i * (OPT_BTN_H + OPT_GAP);
        if (mouse.x >= OPT_START_X && mouse.x < OPT_START_X + OPT_BTN_W &&
            mouse.y >= btnY && mouse.y < btnY + OPT_BTN_H)
        {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                m_selectedOption = i;
            }
        }
    }

    // "Start" button
    int startBtnY = OPT_START_Y + 6 * (OPT_BTN_H + OPT_GAP);
    if (mouse.x >= OPT_START_X && mouse.x < OPT_START_X + OPT_BTN_W &&
        mouse.y >= startBtnY && mouse.y < startBtnY + OPT_BTN_H + 10)
    {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            auto& opt = s_options[m_selectedOption];
            game.selectedRows = opt.rows;
            game.selectedCols = opt.cols;
            // TODO: transition to PlayState (next step)
            // game.ChangeState(std::make_unique<PlayState>());
        }
    }
}

void DifficultyState::Draw(Game& game)
{
    ClearBackground(Color{ 30, 33, 36, 255 });

    int screenH = GetScreenHeight();

    // Title
    DrawText("Choose difficulty", OPT_START_X, 40, 30, WHITE);
    DrawText("How many pieces?", OPT_START_X, 75, 18, LIGHTGRAY);

    // ---- Preview on the left ----
    int previewX = 80;
    int previewY = 120;

    if (m_preview.id != 0)
    {
        // Background frame
        DrawRectangle(previewX - 4, previewY - 4,
                      PREVIEW_SIZE + 8, PREVIEW_SIZE + 8, ColorAlpha(WHITE, 0.2f));

        DrawTexturePro(m_preview,
            Rectangle{ 0, 0, (float)m_preview.width, (float)m_preview.height },
            Rectangle{ (float)previewX, (float)previewY, (float)PREVIEW_SIZE, (float)PREVIEW_SIZE },
            { 0, 0 }, 0.0f, WHITE);

        // Overlay grid preview
        auto& opt = s_options[m_selectedOption];
        float cellW = (float)PREVIEW_SIZE / opt.cols;
        float cellH = (float)PREVIEW_SIZE / opt.rows;
        for (int r = 0; r <= opt.rows; ++r)
        {
            DrawLine(previewX, previewY + (int)(r * cellH),
                     previewX + PREVIEW_SIZE, previewY + (int)(r * cellH),
                     ColorAlpha(WHITE, 0.4f));
        }
        for (int c = 0; c <= opt.cols; ++c)
        {
            DrawLine(previewX + (int)(c * cellW), previewY,
                     previewX + (int)(c * cellW), previewY + PREVIEW_SIZE,
                     ColorAlpha(WHITE, 0.4f));
        }

        DrawText(s_options[m_selectedOption].label,
                 previewX, previewY + PREVIEW_SIZE + 10, 18, SKYBLUE);
    }
    else
    {
        DrawRectangle(previewX, previewY, PREVIEW_SIZE, PREVIEW_SIZE, ColorAlpha(GRAY, 0.3f));
        DrawText("No preview", previewX + 80, previewY + 130, 20, GRAY);
    }

    // ---- Option buttons on the right ----
    for (int i = 0; i < 6; ++i)
    {
        int btnY = OPT_START_Y + i * (OPT_BTN_H + OPT_GAP);
        bool selected = (i == m_selectedOption);

        Color btnColor = selected ? ColorAlpha(SKYBLUE, 0.6f) : ColorAlpha(WHITE, 0.1f);
        Color borderColor = selected ? SKYBLUE : ColorAlpha(WHITE, 0.3f);

        Vector2 mouse = GetMousePosition();
        bool hover = (mouse.x >= OPT_START_X && mouse.x < OPT_START_X + OPT_BTN_W &&
                      mouse.y >= btnY && mouse.y < btnY + OPT_BTN_H);

        if (!selected && hover)
            btnColor = ColorAlpha(WHITE, 0.2f);

        DrawRectangle(OPT_START_X, btnY, OPT_BTN_W, OPT_BTN_H, btnColor);
        DrawRectangleLines(OPT_START_X, btnY, OPT_BTN_W, OPT_BTN_H, borderColor);

        DrawText(s_options[i].label,
                 OPT_START_X + 20, btnY + 14, 20, selected ? WHITE : LIGHTGRAY);
    }

    // "Start" button
    int startBtnY = OPT_START_Y + 6 * (OPT_BTN_H + OPT_GAP);
    bool hoverStart = (GetMousePosition().x >= OPT_START_X &&
                       GetMousePosition().x < OPT_START_X + OPT_BTN_W &&
                       GetMousePosition().y >= startBtnY &&
                       GetMousePosition().y < startBtnY + OPT_BTN_H + 10);

    Color startColor = hoverStart ? ColorAlpha(GREEN, 0.7f) : ColorAlpha(GREEN, 0.4f);
    DrawRectangle(OPT_START_X, startBtnY, OPT_BTN_W, OPT_BTN_H + 10, startColor);
    DrawRectangleLines(OPT_START_X, startBtnY, OPT_BTN_W, OPT_BTN_H + 10, GREEN);
    DrawText("START", OPT_START_X + 80, startBtnY + 14, 22, WHITE);

    // Hint
    DrawText("(PlayState coming in the next step)",
             10, screenH - 30, 14, GRAY);
}
