#include "states/MainMenuState.h"
#include "states/MenuState.h"
#include "states/SettingsState.h"
#include "states/PlayState.h"
#include "core/Game.h"
#include "core/Config.h"
#include "core/SaveManager.h"
#include "core/DrawHelper.h"
#include "ui/Button.h"
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

void MainMenuState::Update(Game& game)
{
    const float btnW = 280, btnH = 56, gap = 12;
    const float startY = 200;
    const float bx = ui::LogW() / 2 - btnW / 2;

    // Continue
    ui::Button btnContinue(bx, startY, btnW, btnH, "Продолжить");
    btnContinue.SetEnabled(m_hasContinue);
    if (btnContinue.Clicked())
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
    ui::Button btnChoose(bx, startY + 1 * (btnH + gap), btnW, btnH, "Выбрать картинку");
    if (btnChoose.Clicked())
    {
        game.ChangeState(std::make_unique<MenuState>());
        return;
    }

    // Settings
    ui::Button btnSettings(bx, startY + 2 * (btnH + gap), btnW, btnH, "Настройки");
    if (btnSettings.Clicked())
    {
        game.ChangeState(std::make_unique<SettingsState>());
        return;
    }

    // Exit
    ui::Button btnExit(bx, startY + 3 * (btnH + gap), btnW, btnH, "Выход");
    if (btnExit.Clicked())
    {
        game.exitRequested = true;
        return;
    }
}

void MainMenuState::Draw(Game& game)
{
    int sw = GetScreenWidth();
    int cX = sw / 2;
    float s = ui::UiScale();

    ClearBackground(Color{ 240, 242, 245, 255 });

    Font& font = game.GetFont();
    Color accent = { 55, 110, 210, 255 };

    // Title
    int titleSize = (int)(50 * s);
    DrawGameText(font, "Jigsaw Puzzle", cX - MeasureGameText(font, "Jigsaw Puzzle", titleSize) / 2, 80, titleSize, Color{ 40, 40, 45, 255 });
    DrawGameText(font, "от develop-puzzle", cX - (int)(85 * s), 130, (int)(16 * s), ColorAlpha(Color{ 150,150,155,255 }, 0.6f));

    // Buttons
    const float btnW = 280, btnH = 56, gap = 12;
    const float startY = 200;
    const float bx = ui::LogW() / 2 - btnW / 2;

    struct Btn { const char* label; bool enabled; };
    Btn btns[4] = {
        { "Продолжить", m_hasContinue },
        { "Выбрать картинку", true },
        { "Настройки", true },
        { "Выход", true },
    };

    for (int i = 0; i < 4; ++i)
    {
        ui::Button b(bx, startY + i * (btnH + gap), btnW, btnH, btns[i].label);
        b.SetEnabled(btns[i].enabled);
        b.SetFontSize(22);
        b.SetColors(Color{ 255, 255, 255, 255 }, ColorAlpha(accent, 0.25f), Color{ 40, 40, 45, 255 });
        b.SetHoverColors(ColorAlpha(accent, 0.08f), accent, Color{ 40, 40, 45, 255 });
        if (!btns[i].enabled)
            b.SetDisabledColors(ColorAlpha(Color{ 150,150,155,255 }, 0.06f),
                                ColorAlpha(Color{ 150,150,155,255 }, 0.15f),
                                Color{ 150, 150, 155, 255 });
        b.Draw(font);
    }

    // Footer
#if !defined(PLATFORM_ANDROID)
    DrawGameText(font, "F2: каркас  |  Esc: выход",
                 (int)(10 * ui::UiScale()), (int)ui::LogH() - 30,
                 (int)(13 * ui::UiScale()), Color{ 150,150,155,255 });
#endif
}
