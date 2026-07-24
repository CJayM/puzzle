#include "states/MainMenuState.h"
#include "states/MenuState.h"
#include "states/SettingsState.h"
#include "states/PlayState.h"
#include "core/Game.h"
#include "core/SaveManager.h"
#include "core/DrawHelper.h"
#include "raylib.h"

#include <memory>

void MainMenuState::Enter(Game&)
{
    m_hasContinue = false;

    // Check if any save files exist
    // Simple check: try to find any .dat files in saves/
    if (DirectoryExists("saves"))
    {
        FilePathList files = LoadDirectoryFiles("saves/");
        for (unsigned int i = 0; i < files.count; ++i)
        {
            std::string name = files.paths[i];
            if (name.size() > 4 && name.substr(name.size() - 4) == ".dat")
            {
                m_hasContinue = true;
                break;
            }
        }
        UnloadDirectoryFiles(files);
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
            // Try last played puzzle first
            int imgIdx = -1, rows = -1, cols = -1;
            if (!SaveManager::GetLastPlayed(imgIdx, rows, cols) ||
                !SaveManager::HasSave(imgIdx, rows, cols))
            {
                // Fallback: scan for any save file
                FilePathList files = LoadDirectoryFiles("saves/");
                for (unsigned int i = 0; i < files.count; ++i)
                {
                    std::string name = files.paths[i];
                    if (name.size() > 4 && name.substr(name.size() - 4) == ".dat" &&
                        name.find("lastplay") == std::string::npos && name.find("settings") == std::string::npos)
                    {
                        auto fname = name.substr(name.rfind('\\') + 1);
                        if (fname.rfind('/') != std::string::npos)
                            fname = fname.substr(fname.rfind('/') + 1);
                        if (sscanf(fname.c_str(), "puzzle_%d_%dx%d.dat", &imgIdx, &rows, &cols) == 3)
                            break;
                    }
                }
                UnloadDirectoryFiles(files);
            }

            if (imgIdx >= 0 && rows > 0 && cols > 0)
            {
                game.selectedImageIndex = imgIdx;
                game.selectedRows = rows;
                game.selectedCols = cols;
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

    ClearBackground(Color{ 25, 28, 32, 255 });

    Font& font = game.GetFont();

    // Title
    DrawGameText(font, "Jigsaw Puzzle", cX - 120, 80, 50, WHITE);
    DrawGameText(font, "by develop-puzzle", cX - 85, 130, 16, ColorAlpha(GRAY, 0.6f));

    // Buttons
    struct Btn { const char* label; bool enabled; };
    Btn btns[4] = {
        { "Continue", m_hasContinue },
        { "Choose Image", true },
        { "Settings", true },
        { "Exit", true },
    };

    for (int i = 0; i < 4; ++i)
    {
        int by = startY + i * (btnH + gap);
        Rectangle r = btnR((float)(cX - btnW / 2), (float)by, (float)btnW, (float)btnH);
        bool en = btns[i].enabled;
        bool hv = en && btnHov(r);

        Color bg = en ? (hv ? ColorAlpha(SKYBLUE, 0.5f) : ColorAlpha(WHITE, 0.1f))
                      : ColorAlpha(GRAY, 0.15f);
        Color border = en ? (hv ? SKYBLUE : ColorAlpha(WHITE, 0.3f))
                          : ColorAlpha(GRAY, 0.2f);

        DrawRectangleRec(r, bg);
        DrawRectangleLinesEx(r, 1, border);
        DrawGameText(font, btns[i].label, cX - MeasureGameText(font, btns[i].label, 22) / 2,
                     by + 16, 22, en ? WHITE : GRAY);
    }

    // Footer
    DrawGameText(font, "F2: wireframe  |  Esc: exit", 10, sh - 30, 14, GRAY);
}
