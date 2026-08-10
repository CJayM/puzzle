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

    InitButtons();
}

void MainMenuState::Exit(Game&)
{
}

// Menu buttons: labels, styles, vertical layout — created once.
// Sizes are auto (from the label) and refreshed in UpdateButtons().
void MainMenuState::InitButtons()
{
    const Color accent = { 55, 110, 210, 255 };

    m_btnContinue.SetLabel("Продолжить");
    m_btnChoose.SetLabel("Выбрать картинку");
    m_btnSettings.SetLabel("Настройки");
    m_btnExit.SetLabel("Выход");

    auto style = [&](ui::Button& b) {
        b.SetFontSize(22);
        b.SetColors(Color{ 255, 255, 255, 255 }, ColorAlpha(accent, 0.25f), Color{ 40, 40, 45, 255 });
        b.SetHoverColors(ColorAlpha(accent, 0.08f), accent, Color{ 40, 40, 45, 255 });
    };
    style(m_btnContinue); style(m_btnChoose); style(m_btnSettings); style(m_btnExit);
    m_btnContinue.SetDisabledColors(ColorAlpha(Color{ 150,150,155,255 }, 0.06f),
                                    ColorAlpha(Color{ 150,150,155,255 }, 0.15f),
                                    Color{ 150, 150, 155, 255 });

    m_layout = ui::VLayout();
    m_layout.SetSpacing(12);
    m_layout.AddItem(&m_btnContinue);
    m_layout.AddItem(&m_btnChoose);
    m_layout.AddItem(&m_btnSettings);
    m_layout.AddItem(&m_btnExit);
}

// Refresh per-frame state: auto sizes, enabled + layout positions
void MainMenuState::UpdateButtons(Font& font)
{
    const float gap = 12.0f;
    const float startY = 200;

    m_btnContinue.SetEnabled(m_hasContinue);

    float w = 0, h = 0;
    for (auto* b : { &m_btnContinue, &m_btnChoose, &m_btnSettings, &m_btnExit })
    {
        b->UpdateAutoSize(font);
        w = std::max(w, b->MinWidth());
        h = std::max(h, b->MinHeight());
    }

    m_layout.Compute(ui::LogW() / 2 - w / 2, startY, w, 4 * h + 3 * gap);
}

void MainMenuState::Update(Game& game)
{
    UpdateButtons(game.GetFont());

    if (m_btnContinue.Clicked())
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

    if (m_btnChoose.Clicked())
    {
        game.ChangeState(std::make_unique<MenuState>());
        return;
    }

    if (m_btnSettings.Clicked())
    {
        game.ChangeState(std::make_unique<SettingsState>());
        return;
    }

    if (m_btnExit.Clicked())
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

    // Title
    int titleSize = (int)(50 * s);
    DrawGameText(font, "Jigsaw Puzzle", cX - MeasureGameText(font, "Jigsaw Puzzle", titleSize) / 2, 80, titleSize, Color{ 40, 40, 45, 255 });

    // Buttons — vertical layout, same light style for all
    m_btnContinue.Draw(font);
    m_btnChoose.Draw(font);
    m_btnSettings.Draw(font);
    m_btnExit.Draw(font);
}
