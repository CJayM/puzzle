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
            // Get current difficulty preset from settings
            int preset = SettingsState::GetSettings().difficultyPreset;
            int targetRows = 0, targetCols = 0;
            // Map preset index to rows/cols
            switch (preset) {
                case 0: targetRows = 5;  targetCols = 5;  break;
                case 1: targetRows = 8;  targetCols = 8;  break;
                case 2: targetRows = 16; targetCols = 16; break;
                case 3: targetRows = 32; targetCols = 32; break;
                default: targetRows = 8; targetCols = 8;  break;
            }

            // Try last played puzzle first (must match current difficulty)
            int imgIdx = -1, rows = -1, cols = -1;
            if (SaveManager::GetLastPlayed(imgIdx, rows, cols) &&
                rows == targetRows && cols == targetCols &&
                SaveManager::HasSave(imgIdx, rows, cols))
            {
                // Check if puzzle is already complete
                float cs, tm;
                std::vector<Piece> testPieces;
                if (SaveManager::Load(imgIdx, rows, cols, cs, tm, testPieces))
                {
                    int firstBlock = testPieces.empty() ? -1 : testPieces[0].blockId;
                    bool allSameBlock = true;
                    for (auto& p : testPieces)
                        if (p.blockId != firstBlock) { allSameBlock = false; break; }

                    if (allSameBlock && testPieces.size() > 1)
                    {
                        SaveManager::DeleteSave(imgIdx, rows, cols);
                        imgIdx = -1;
                    }
                }
            }
            else
            {
                imgIdx = -1;
            }

            if (imgIdx < 0)
            {
                // Fallback: scan for any uncompleted save matching current difficulty
                FilePathList files = LoadDirectoryFiles("saves/");
                for (unsigned int i = 0; i < files.count; ++i)
                {
                    std::string name = files.paths[i];
                    if (name.size() > 4 && name.substr(name.size() - 4) == ".dat" &&
                        name.find("lastplay") == std::string::npos &&
                        name.find("puzzle_") != std::string::npos)
                    {
                        auto fname = name.substr(name.rfind('\\') + 1);
                        if (fname.rfind('/') != std::string::npos)
                            fname = fname.substr(fname.rfind('/') + 1);
                        int ii = -1, rr = -1, cc = -1;
                        if (sscanf(fname.c_str(), "puzzle_%d_%dx%d.dat", &ii, &rr, &cc) == 3 &&
                            rr == targetRows && cc == targetCols)
                        {
                            // Check if not complete
                            float cs2, tm2;
                            std::vector<Piece> tp;
                            if (SaveManager::Load(ii, rr, cc, cs2, tm2, tp))
                            {
                                int fb = tp.empty() ? -1 : tp[0].blockId;
                                bool complete = true;
                                for (auto& p : tp)
                                    if (p.blockId != fb) { complete = false; break; }
                                if (!complete && tp.size() > 1)
                                { imgIdx = ii; rows = rr; cols = cc; break; }
                            }
                        }
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

    ClearBackground(Color{ 240, 242, 245, 255 });

    Font& font = game.GetFont();
    Color accent = { 55, 110, 210, 255 };

    // Title
    DrawGameText(font, "Jigsaw Puzzle", cX - 120, 80, 50, Color{ 40, 40, 45, 255 });
    DrawGameText(font, "by develop-puzzle", cX - 85, 130, 16, ColorAlpha(Color{ 150,150,155,255 }, 0.6f));

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
    DrawGameText(font, "F2: wireframe  |  Esc: exit", 10, sh - 30, 13, Color{ 150,150,155,255 });
}
