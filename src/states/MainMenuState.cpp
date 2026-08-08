#include "states/MainMenuState.h"
#include "states/MenuState.h"
#include "states/SettingsState.h"
#include "states/PlayState.h"
#include "core/Game.h"
#include "core/Config.h"
#include "core/SaveManager.h"
#include "core/DrawHelper.h"
#include "raylib.h"

#include <algorithm>
#include <memory>

void MainMenuState::Enter(Game&)
{
    // Continue is enabled only when the last played image has an
    // unfinished save (blocked once that puzzle is fully assembled).
    // NOTE: no FileExists() check — on Android the image lives inside the
    // APK assets where access()/stat() can't see it; HasSave() checks the
    // save file (real path in internal storage), which is sufficient.
    m_hasContinue = false;
    std::string lp;
    int lr = 0, lc = 0;
    if (SaveManager::GetLastPlayed(lp, lr, lc) &&
        SaveManager::HasSave(lp, lr, lc))
    {
        float elapsed = 0;
        int pc = 0, bc = 0;
        if (SaveManager::LoadHeader(lp, lr, lc, elapsed, pc, bc) && pc > 1)
        {
            float pct = std::clamp((float)(pc - bc) / (float)(pc - 1), 0.0f, 1.0f);
            if (pct < 1.0f) m_hasContinue = true;
        }
    }
}

void MainMenuState::Exit(Game&)
{
}

static Rectangle btnR(float x, float y, float w, float h)
{
    return Rectangle{ x, y, w, h };
}

static bool btnHov(Rectangle r)
{
    return CheckCollisionPointRec(GetMousePosition(), r);
}

void MainMenuState::Update(Game& game)
{
    int sw = GetScreenWidth();
    int cX = sw / 2;
    int btnW = 280;
    int btnH = 56;
    int gap = 12;
    int startY = 200;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        // Continue
        Rectangle r = btnR((float)(cX - btnW / 2), (float)startY, (float)btnW, (float)btnH);
        if (btnHov(r) && m_hasContinue)
        {
            // Resume the last played image (guaranteed unfinished by Enter())
            std::string lp;
            int lr = 0, lc = 0;
            if (SaveManager::GetLastPlayed(lp, lr, lc))
            {
                game.selectedImagePath = lp;
                game.selectedRows = lr;
                game.selectedCols = lc;
                game.continueGame = true;
                game.ChangeState(std::make_unique<PlayState>());
                return;
            }
        }

        // Choose Image
        Rectangle r2 = btnR((float)(cX - btnW / 2), (float)(startY + 1 * (btnH + gap)),
                            (float)btnW, (float)btnH);
        if (btnHov(r2))
        {
            game.ChangeState(std::make_unique<MenuState>());
            return;
        }

        // Settings
        Rectangle r3 = btnR((float)(cX - btnW / 2), (float)(startY + 2 * (btnH + gap)),
                            (float)btnW, (float)btnH);
        if (btnHov(r3))
        {
            game.ChangeState(std::make_unique<SettingsState>());
            return;
        }

        // Exit
        Rectangle r4 = btnR((float)(cX - btnW / 2), (float)(startY + 3 * (btnH + gap)),
                            (float)btnW, (float)btnH);
        if (btnHov(r4))
        {
            game.exitRequested = true;
            return;
        }
    }
}

void MainMenuState::Draw(Game& game)
{
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    int cX = sw / 2;
    int btnW = 280;
    int btnH = 56;
    int gap = 12;
    int startY = 200;

    ClearBackground(Color{ 240, 242, 245, 255 });

    Font& font = game.GetFont();
    Color accent = { 55, 110, 210, 255 };

    // Title
    DrawGameText(font, "Jigsaw Puzzle", cX - 120, 80, 50, Color{ 40, 40, 45, 255 });
    DrawGameText(font, "от develop-puzzle", cX - 85, 130, 16, ColorAlpha(Color{ 150,150,155,255 }, 0.6f));

    // Buttons
    struct Btn { const char* label; bool enabled; };
    Btn btns[4] = {
        { "Продолжить", m_hasContinue },
        { "Выбрать картинку", true },
        { "Настройки", true },
        { "Выход", true },
    };

    for (int i = 0; i < 4; ++i)
    {
        int by = startY + i * (btnH + gap);
        Rectangle r = btnR((float)(cX - btnW / 2), (float)by, (float)btnW, (float)btnH);
        bool en = btns[i].enabled;
        bool hv = en && btnHov(r);

        Color bg = { 255, 255, 255, 255 };
        Color border = ColorAlpha(accent, 0.25f);
        Color textCol = { 40, 40, 45, 255 };
        if (!en) {
            bg = ColorAlpha({ 150,150,155,255 }, 0.06f);
            border = ColorAlpha({ 150,150,155,255 }, 0.15f);
            textCol = { 150, 150, 155, 255 };
        } else if (hv) {
            bg = ColorAlpha(accent, 0.08f);
            border = accent;
        }

        DrawRectangleRounded(r, 6, 1, bg);
        DrawRectangleRoundedLines(r, 6, 1, border);
        DrawGameText(font, btns[i].label, cX - MeasureGameText(font, btns[i].label, 22) / 2,
                     by + 15, 22, textCol);
    }

    // Footer
#if !defined(PLATFORM_ANDROID)
    DrawGameText(font, "F2: каркас  |  Esc: выход", 10, sh - 30, 13, Color{ 150,150,155,255 });
#endif
}
