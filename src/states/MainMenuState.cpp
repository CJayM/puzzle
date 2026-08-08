#include "states/MainMenuState.h"
#include "states/MenuState.h"
#include "states/SettingsState.h"
#include "states/PlayState.h"
#include "core/Game.h"
#include "core/Config.h"
#include "core/SaveManager.h"
#include "core/DrawHelper.h"
#include "raylib.h"

#include <memory>

void MainMenuState::Enter(Game&)
{
    m_hasContinue = false;

    // Check if any save files exist. The save dir differs per platform
    // (internal storage on Android), so ask SaveManager for the real path.
    if (SaveManager::HasAnySave())
    {
        FilePathList files = LoadDirectoryFiles(Config::Instance().saveDir.c_str());
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
            std::string path;
            int rows = -1, cols = -1;
            if (SaveManager::GetLastPlayed(path, rows, cols) &&
                rows == targetRows && cols == targetCols &&
                FileExists(path.c_str()) &&
                SaveManager::HasSave(path, rows, cols))
            {
                // Check if puzzle is already complete
                float cs, tm;
                std::vector<Piece> testPieces;
                if (SaveManager::Load(path, rows, cols, cs, tm, testPieces))
                {
                    int firstBlock = testPieces.empty() ? -1 : testPieces[0].blockId;
                    bool allSameBlock = true;
                    for (auto& p : testPieces)
                        if (p.blockId != firstBlock) { allSameBlock = false; break; }

                    if (allSameBlock && testPieces.size() > 1)
                    {
                        SaveManager::DeleteSave(path, rows, cols);
                        path.clear();
                    }
                }
            }
            else
            {
                path.clear();
            }

            if (path.empty())
            {
                // Fallback: scan save files, read each header to find a matching
                // (rows x cols) unfinished puzzle whose image still exists
                FilePathList files = LoadDirectoryFiles(Config::Instance().saveDir.c_str());
                for (unsigned int i = 0; i < files.count; ++i)
                {
                    std::string name = files.paths[i];
                    if (name.size() > 4 && name.substr(name.size() - 4) == ".dat" &&
                        name.find("lastplay") == std::string::npos)
                    {
                        FILE* f = fopen(name.c_str(), "rb");
                        if (!f) continue;
                        SaveHeader h;
                        bool ok = (fread(&h, sizeof(h), 1, f) == 1 &&
                                  h.magic[0] == 'J' && h.magic[1] == 'P' &&
                                  h.magic[2] == 'Z' && h.magic[3] == 2);
                        fclose(f);
                        if (!ok) continue;

                        if (h.rows == targetRows && h.cols == targetCols &&
                            h.imagePath[0] != 0 && FileExists(h.imagePath))
                        {
                            // Check if not complete
                            float cs2, tm2;
                            std::vector<Piece> tp;
                            if (SaveManager::Load(h.imagePath, h.rows, h.cols, cs2, tm2, tp))
                            {
                                int fb = tp.empty() ? -1 : tp[0].blockId;
                                bool complete = true;
                                for (auto& p : tp)
                                    if (p.blockId != fb) { complete = false; break; }
                                if (!complete && tp.size() > 1)
                                { path = h.imagePath; rows = h.rows; cols = h.cols; break; }
                            }
                        }
                    }
                }
                UnloadDirectoryFiles(files);
            }

            if (!path.empty() && rows > 0 && cols > 0)
            {
                game.selectedImagePath = path;
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
