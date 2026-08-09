#include "states/CompletedState.h"
#include "states/MainMenuState.h"
#include "states/MenuState.h"
#include "states/PlayState.h"
#include "states/SettingsState.h"
#include "core/Game.h"
#include "core/DrawHelper.h"
#include "ui/Button.h"
#include "raylib.h"

#include <memory>

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
    const float btnW = 200, btnH = 50;
    const float startX = ui::LogW() / 2 - btnW / 2;
    const float startY = ui::LogH() / 2 + 40;

    ui::Button btnAgain(startX, startY, btnW, btnH, "Играть снова");
    if (btnAgain.Clicked())
    {
        // Restart the same puzzle with a fresh scramble
        game.continueGame = false;
        game.ChangeState(std::make_unique<PlayState>());
        return;
    }

    ui::Button btnNew(startX, startY + 65, btnW, btnH, "Другая картинка");
    if (btnNew.Clicked())
    {
        // Go to the first not-yet-assembled image
        MenuState::SetFirstUnfinished();
        game.ChangeState(std::make_unique<MenuState>());
        return;
    }

    ui::Button btnMenu(startX, startY + 130, btnW, btnH, "В меню");
    if (btnMenu.Clicked())
    {
        game.ChangeState(std::make_unique<MainMenuState>());
        return;
    }
}

void CompletedState::Draw(Game& game)
{
    float s = ui::UiScale();
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
    const float btnW = 200, btnH = 50;
    const float startX = ui::LogW() / 2 - btnW / 2;
    const float startY = ui::LogH() / 2 + 40;

    struct BtnSpec { const char* label; Color bg; };
    BtnSpec specs[3] = {
        { "Играть снова", DARKGREEN },
        { "Другая картинка", Color{ 40, 80, 120, 255 } },
        { "В меню", Color{ 60, 40, 40, 255 } },
    };
    for (int i = 0; i < 3; ++i)
    {
        ui::Button b(startX, startY + i * 65, btnW, btnH, specs[i].label);
        b.SetColors(specs[i].bg, ColorAlpha(WHITE, 0.6f), WHITE);
        b.SetHoverColors(specs[i].bg, WHITE, WHITE);
        b.Draw(font);
    }
}
