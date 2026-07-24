#include "states/CompletedState.h"
#include "states/MenuState.h"
#include "core/Game.h"
#include "raylib.h"

#include <memory>

static Rectangle btnRect(float x, float y, float w, float h)
{
    return Rectangle{ x, y, w, h };
}

static bool btnHover(Rectangle r)
{
    return CheckCollisionPointRec(GetMousePosition(), r);
}

static void drawBtn(Rectangle r, const char* label, Color bg, bool enabled)
{
    Color c = enabled ? bg : ColorAlpha(bg, 0.3f);
    DrawRectangleRec(r, c);
    DrawRectangleLinesEx(r, 1, enabled ? ColorAlpha(WHITE, 0.6f) : ColorAlpha(WHITE, 0.2f));
    DrawText(label, (int)r.x + 10, (int)r.y + 10, 18, enabled ? WHITE : GRAY);
}

void CompletedState::Enter(Game& game)
{
    // We don't have direct access to the source texture from PlayState,
    // so we load the original image fresh
    const auto& images = game.GetResources().GetImageList();
    if (game.selectedImageIndex >= 0 &&
        game.selectedImageIndex < (int)images.size())
    {
        Image img = LoadImage(images[game.selectedImageIndex].filePath.c_str());
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
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    int btnW = 200, btnH = 50;
    int startX = sw / 2 - btnW / 2;
    int startY = sh / 2 + 40;

    Rectangle btnAgain  = btnRect((float)startX, (float)startY, (float)btnW, (float)btnH);
    Rectangle btnNew    = btnRect((float)startX, (float)(startY + 65), (float)btnW, (float)btnH);
    Rectangle btnMenu   = btnRect((float)startX, (float)(startY + 130), (float)btnW, (float)btnH);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        if (btnHover(btnAgain))
        {
            // Stay in completed state but restart: go back to DifficultyState
            // For now, go to menu
            game.ChangeState(std::make_unique<MenuState>());
        }
        else if (btnHover(btnNew) || btnHover(btnMenu))
        {
            game.ChangeState(std::make_unique<MenuState>());
        }
    }
}

void CompletedState::Draw(Game&)
{
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
        float drawY = 50.0f;

        DrawTexturePro(m_completedTexture,
            Rectangle{ 0, 0, texW, texH },
            Rectangle{ drawX, drawY, drawW, drawH },
            Vector2{ 0, 0 }, 0.0f, WHITE);
    }

    // "Puzzle Complete!" text
    DrawText("Puzzle Complete!",
             sw / 2 - MeasureText("Puzzle Complete!", 40) / 2,
             12, 40, GOLD);

    // Buttons
    int btnW = 200, btnH = 50;
    int startX = sw / 2 - btnW / 2;
    int startY = sh / 2 + 40;

    drawBtn(btnRect((float)startX, (float)startY, (float)btnW, (float)btnH),
            "Play Again", DARKGREEN, true);
    drawBtn(btnRect((float)startX, (float)(startY + 65), (float)btnW, (float)btnH),
            "New Image", Color{ 40, 80, 120, 255 }, true);
    drawBtn(btnRect((float)startX, (float)(startY + 130), (float)btnW, (float)btnH),
            "Menu", Color{ 60, 40, 40, 255 }, true);
}
