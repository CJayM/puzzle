#include "states/CompletedState.h"
#include "states/MainMenuState.h"
#include "states/MenuState.h"
#include "states/PlayState.h"
#include "states/SettingsState.h"
#include "core/Game.h"
#include "core/Config.h"
#include "core/DrawHelper.h"
#include "raylib.h"

#include <memory>

// Virtual pixels: sizes below are in dp, scaled by device density and the
// user's UI-size preference, so buttons/labels match on any screen.
static float UiScale() { return Config::Instance().dpiScale * SettingsState::UserScale(); }
static float LogW()    { return GetScreenWidth() / UiScale(); }
static float LogH()    { return GetScreenHeight() / UiScale(); }

static Rectangle btnRect(float x, float y, float w, float h)
{
    float s = UiScale();
    return Rectangle{ x * s, y * s, w * s, h * s };
}

static bool btnHover(Rectangle r)
{
    return CheckCollisionPointRec(GetMousePosition(), r);
}

static void drawBtn(Font& font, Rectangle r, const char* label, Color bg, bool enabled)
{
    Color c = enabled ? bg : ColorAlpha(bg, 0.3f);
    DrawRectangleRec(r, c);
    DrawRectangleLinesEx(r, 1, enabled ? ColorAlpha(WHITE, 0.6f) : ColorAlpha(WHITE, 0.2f));
    DrawGameText(font, label, (int)r.x + (int)(10 * UiScale()), (int)r.y + (int)(10 * UiScale()),
                 (int)(18 * UiScale()), enabled ? WHITE : GRAY);
}

void CompletedState::Enter(Game& game)
{
    // We don't have direct access to the source texture from PlayState,
    // so we load the original image fresh
    if (!game.selectedImagePath.empty())
    {
        Image img = LoadImage(game.selectedImagePath.c_str());
        if (img.data != nullptr)
        {
            // Downscale for display
            int maxSize = 600;
            if (img.width > maxSize || img.height > maxSize)
            {
                float scale = (float)maxSize / (float)std::max(img.width, img.height);
                ImageResize(&img, (int)(img.width * scale), (int)(img.height * scale));
            }
            m_completedTexture = LoadTextureFromImage(img);
            UnloadImage(img);
        }
    }
}

void CompletedState::Exit(Game&)
{
    if (m_completedTexture.id != 0)
        UnloadTexture(m_completedTexture);
}

void CompletedState::Update(Game& game)
{
    Vector2 mouse = GetMousePosition();

    int btnW = 200, btnH = 50;
    int startX = (int)LogW() / 2 - btnW / 2;
    int startY = (int)LogH() / 2 + 40;

    Rectangle btnAgain  = btnRect((float)startX, (float)startY, (float)btnW, (float)btnH);
    Rectangle btnNew    = btnRect((float)startX, (float)(startY + 65), (float)btnW, (float)btnH);
    Rectangle btnMenu   = btnRect((float)startX, (float)(startY + 130), (float)btnW, (float)btnH);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        if (btnHover(btnAgain))
        {
            // Restart the same puzzle with a fresh scramble
            game.continueGame = false;
            game.ChangeState(std::make_unique<PlayState>());
        }
        else if (btnHover(btnNew))
        {
            // Go to the first not-yet-assembled image
            MenuState::SetFirstUnfinished();
            game.ChangeState(std::make_unique<MenuState>());
        }
        else if (btnHover(btnMenu))
        {
            game.ChangeState(std::make_unique<MainMenuState>());
        }
    }
}

void CompletedState::Draw(Game& game)
{
    float s = UiScale();
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    ClearBackground(Color{ 20, 25, 30, 255 });

    // Draw the completed image if available
    if (m_completedTexture.id != 0)
    {
        float texW = (float)m_completedTexture.width;
        float texH = (float)m_completedTexture.height;

        float scale = 1.0f;
        if (texW > sw * 0.7f || texH > sh * 0.45f)
        {
            float sx = (sw * 0.7f) / texW;
            float sy = (sh * 0.45f) / texH;
            scale = std::min(sx, sy);
        }

        float drawW = texW * scale;
        float drawH = texH * scale;
        float drawX = (sw - drawW) / 2.0f;
        float drawY = 50.0f * s;

        DrawTexturePro(m_completedTexture,
            Rectangle{ 0, 0, texW, texH },
            Rectangle{ drawX, drawY, drawW, drawH },
            Vector2{ 0, 0 }, 0.0f, WHITE);
    }

    Font& font = game.GetFont();

    // "Puzzle Complete!" text
    int titleSize = (int)(40 * s);
    DrawGameText(font, "Пазл собран!",
             sw / 2 - MeasureGameText(font, "Пазл собран!", titleSize) / 2,
             (int)(12 * s), titleSize, GOLD);

    // Buttons
    int btnW = 200, btnH = 50;
    int startX = (int)LogW() / 2 - btnW / 2;
    int startY = (int)LogH() / 2 + 40;

    drawBtn(font, btnRect((float)startX, (float)startY, (float)btnW, (float)btnH),
            "Играть снова", DARKGREEN, true);
    drawBtn(font, btnRect((float)startX, (float)(startY + 65), (float)btnW, (float)btnH),
            "Другая картинка", Color{ 40, 80, 120, 255 }, true);
    drawBtn(font, btnRect((float)startX, (float)(startY + 130), (float)btnW, (float)btnH),
            "В меню", Color{ 60, 40, 40, 255 }, true);
}
