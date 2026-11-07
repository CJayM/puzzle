#include "states/DifficultyState.h"
#include "states/MainMenuState.h"
#include "states/PlayState.h"
#include "core/Game.h"
#include "core/DrawHelper.h"
#include "core/SaveManager.h"
#include "puzzle/PuzzleGenerator.h"
#include "raylib.h"

#include <algorithm>
#include <memory>

const DifficultyOption DifficultyState::s_options[6] = {
    { "9 кусочков",   3, 3, 9  },
    { "16 кусочков",  4, 4, 16 },
    { "25 кусочков",  5, 5, 25 },
    { "36 кусочков",  6, 6, 36 },
    { "49 кусочков",  7, 7, 49 },
    { "64 кусочка",   8, 8, 64 },
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
    m_imageW = 0;
    m_imageH = 0;

    // Load preview thumbnail
    if (!game.selectedImagePath.empty())
    {
        Image img = LoadImage(game.selectedImagePath.c_str());
        if (img.data)
        {
            // Исходные размеры нужны, чтобы подобрать сетку под пропорции
            m_imageW = img.width;
            m_imageH = img.height;

            if (img.width > PREVIEW_SIZE || img.height > PREVIEW_SIZE)
            {
                float s = (float)PREVIEW_SIZE / std::max(img.width, img.height);
                ImageResize(&img, (int)(img.width * s), (int)(img.height * s));
            }
            m_preview = LoadTextureFromImage(img);
            SetTextureFilter(m_preview, TEXTURE_FILTER_BILINEAR);
            UnloadImage(img);
        }
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
            game.continueGame = SaveManager::HasSave(
                game.selectedImagePath, opt.rows, opt.cols);
            game.ChangeState(std::make_unique<PlayState>());
        }
    }

    // "Back" button
    int backBtnY = startBtnY + OPT_BTN_H + 20;
    if (mouse.x >= OPT_START_X && mouse.x < OPT_START_X + OPT_BTN_W &&
        mouse.y >= backBtnY && mouse.y < backBtnY + OPT_BTN_H - 10)
    {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            game.ChangeState(std::make_unique<MainMenuState>());
        }
    }
}

void DifficultyState::Draw(Game& game)
{
    ClearBackground(Color{ 30, 33, 36, 255 });

    int screenH = GetScreenHeight();
    Font& font = game.GetFont();

    // Title
    DrawGameText(font, "Выберите сложность", OPT_START_X, 40, 30, WHITE);
    DrawGameText(font, "Сколько кусочков?", OPT_START_X, 75, 18, LIGHTGRAY);

    // ---- Preview on the left ----
    int previewX = 80;
    int previewY = 120;

    if (m_preview.id != 0)
    {
        // Превью вписывается в квадрат с сохранением пропорций картинки:
        // иначе кадр 16:9 рисовался бы растянутым до 1:1
        const float fit = (float)PREVIEW_SIZE /
                          (float)std::max(m_preview.width, m_preview.height);
        const float drawW = (float)m_preview.width * fit;
        const float drawH = (float)m_preview.height * fit;
        const float drawX = (float)previewX + ((float)PREVIEW_SIZE - drawW) * 0.5f;
        const float drawY = (float)previewY + ((float)PREVIEW_SIZE - drawH) * 0.5f;

        // Background frame
        DrawRectangle((int)drawX - 4, (int)drawY - 4,
                      (int)drawW + 8, (int)drawH + 8, ColorAlpha(WHITE, 0.2f));

        DrawTexturePro(m_preview,
            Rectangle{ 0, 0, (float)m_preview.width, (float)m_preview.height },
            Rectangle{ drawX, drawY, drawW, drawH },
            { 0, 0 }, 0.0f, WHITE);

        // Overlay grid preview — по границам изображения, с той сеткой,
        // которая реально получится для этой картинки
        auto& opt = s_options[m_selectedOption];
        int gridRows = opt.rows, gridCols = opt.cols;
        if (m_imageW > 0 && m_imageH > 0)
            PuzzleGenerator::ChooseGrid(opt.rows, opt.cols, m_imageW, m_imageH,
                                        gridRows, gridCols);

        float cellW = drawW / (float)gridCols;
        float cellH = drawH / (float)gridRows;
        for (int r = 0; r <= gridRows; ++r)
        {
            DrawLine((int)drawX, (int)(drawY + r * cellH),
                     (int)(drawX + drawW), (int)(drawY + r * cellH),
                     ColorAlpha(WHITE, 0.4f));
        }
        for (int c = 0; c <= gridCols; ++c)
        {
            DrawLine((int)(drawX + c * cellW), (int)drawY,
                     (int)(drawX + c * cellW), (int)(drawY + drawH),
                     ColorAlpha(WHITE, 0.4f));
        }

        DrawGameText(font, opt.label,
                     previewX, previewY + PREVIEW_SIZE + 10, 18, SKYBLUE);
        DrawGameText(font, TextFormat("Сетка: %d x %d", gridCols, gridRows),
                     previewX, previewY + PREVIEW_SIZE + 34, 16, LIGHTGRAY);
    }
    else
    {
        DrawRectangle(previewX, previewY, PREVIEW_SIZE, PREVIEW_SIZE, ColorAlpha(GRAY, 0.3f));
        DrawGameText(font, "Нет превью", previewX + 80, previewY + 130, 20, GRAY);
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

        DrawGameText(font, s_options[i].label,
                 OPT_START_X + 20, btnY + 14, 20, selected ? WHITE : LIGHTGRAY);
    }

    // "Start" button
    auto& opt = s_options[m_selectedOption];
    bool hasSave = SaveManager::HasSave(game.selectedImagePath, opt.rows, opt.cols);
    const char* startLabel = hasSave ? "Продолжить" : "НАЧАТЬ";

    int startBtnY = OPT_START_Y + 6 * (OPT_BTN_H + OPT_GAP);
    bool hoverStart = (GetMousePosition().x >= OPT_START_X &&
                       GetMousePosition().x < OPT_START_X + OPT_BTN_W &&
                       GetMousePosition().y >= startBtnY &&
                       GetMousePosition().y < startBtnY + OPT_BTN_H + 10);

    Color startColor = hoverStart ? ColorAlpha(GREEN, 0.7f) : ColorAlpha(GREEN, 0.4f);
    if (hasSave) startColor = hoverStart ? ColorAlpha(SKYBLUE, 0.7f) : ColorAlpha(SKYBLUE, 0.4f);

    DrawRectangle(OPT_START_X, startBtnY, OPT_BTN_W, OPT_BTN_H + 10, startColor);
    DrawRectangleLines(OPT_START_X, startBtnY, OPT_BTN_W, OPT_BTN_H + 10,
                       hasSave ? SKYBLUE : GREEN);
    DrawGameText(font, startLabel,
                 OPT_START_X + (OPT_BTN_W - MeasureGameText(font, startLabel, 22)) / 2,
                 startBtnY + 14, 22, WHITE);

    // "Back" button
    int backBtnY = startBtnY + OPT_BTN_H + 20;
    bool hoverBack = (GetMousePosition().x >= OPT_START_X &&
                      GetMousePosition().x < OPT_START_X + OPT_BTN_W &&
                      GetMousePosition().y >= backBtnY &&
                      GetMousePosition().y < backBtnY + OPT_BTN_H - 10);
    Color backColor = hoverBack ? ColorAlpha(ORANGE, 0.6f) : ColorAlpha(ORANGE, 0.3f);
    DrawRectangle(OPT_START_X, backBtnY, OPT_BTN_W, OPT_BTN_H - 10, backColor);
    DrawRectangleLines(OPT_START_X, backBtnY, OPT_BTN_W, OPT_BTN_H - 10, ORANGE);
    DrawGameText(font, "Назад",
                 OPT_START_X + (OPT_BTN_W - MeasureGameText(font, "Назад", 20)) / 2,
                 backBtnY + 8, 20, WHITE);
}
