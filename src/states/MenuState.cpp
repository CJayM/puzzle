#include "states/MenuState.h"
#include "states/PlayState.h"
#include "states/SettingsState.h"
#include "states/MainMenuState.h"
#include "core/Game.h"
#include "core/DrawHelper.h"
#include "core/SaveManager.h"
#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>

// List (categories / subcategories) layout
static const int ROW_H = 42;
static const int LIST_W = 400;
static const int LIST_X = 60;
static const int LIST_Y = 120;

// Tile grid (image level) — left half of the window, scales with width
static const int TILE_COLS = 6;
static int TileW()   { return GetScreenWidth() / 13; }             // ~98 @1280
static int TileH()   { return TileW(); }
static int TileGap() { return std::max(2, TileW() / 16); }         // ~6 @1280
static int GridX()   { return GetScreenWidth() / 80; }             // 16 @1280
static int GridW()   { return TILE_COLS * TileW() + (TILE_COLS - 1) * TileGap(); }

// Right half: big preview + "Начать" button overlaid on top
static float UiScale() { return (float)GetScreenWidth() / 1280.0f; }

static int PrevCenterX() { return GetScreenWidth() * 3 / 4; }

static const Color BG   = { 240, 242, 245, 255 };
static const Color CARD = { 255, 255, 255, 255 };
static const Color TEXT = { 40, 40, 45, 255 };
static const Color MUT  = { 150, 150, 155, 255 };
static const Color ACC  = { 55, 110, 210, 255 };
static const Color ACCL = { 220, 235, 255, 255 };
static const Color HOV  = { 235, 240, 250, 255 };

// Map preset index → rows/cols
static void GetDiffFromPreset(int preset, int& rows, int& cols)
{
    switch (preset) {
        case 0: rows=5; cols=5; break;
        case 1: rows=8; cols=8; break;
        case 2: rows=16; cols=16; break;
        case 3: rows=32; cols=32; break;
        default: rows=8; cols=8; break;
    }
}

// Big preview square on the right half of the window
static int PrevMax() { return GetScreenWidth() * 7 / 16; }  // ~560 @1280

static Rectangle PreviewRect()
{
    int sh = GetScreenHeight();
    int pz = std::min(PrevMax(), sh - 190);
    int px = PrevCenterX() - pz / 2;
    int py = (sh - pz) / 2 - 10;
    return { (float)px, (float)py, (float)pz, (float)pz };
}

bool MenuState::s_firstUnfinished = false;

void MenuState::Enter(Game& game)
{
    m_level = 0;
    m_selCat = -1;
    m_selSub = -1;
    m_selectedIndex = -1;
    m_scrollOffset = 0;
    m_preview = { 0 };
    m_previewKey.clear();
    m_progress.clear();

    if (s_firstUnfinished)
    {
        s_firstUnfinished = false;
        SelectFirstUnfinished(game);
    }
}

void MenuState::Exit(Game&)
{
    if (m_preview.id != 0) { UnloadTexture(m_preview); m_preview = { 0 }; }
}

// Number of rows in the current level's list
int MenuState::LevelCount(Game& game) const
{
    const auto& catalog = game.GetResources().GetCatalog();
    switch (m_level)
    {
        case 0: return (int)catalog.size();
        case 1:
            if (m_selCat < 0 || m_selCat >= (int)catalog.size()) return 0;
            return (int)catalog[m_selCat].subcategories.size();
        case 2:
            if (m_selCat < 0 || m_selCat >= (int)catalog.size()) return 0;
            if (m_selSub < 0 || m_selSub >= (int)catalog[m_selCat].subcategories.size()) return 0;
            return (int)catalog[m_selCat].subcategories[m_selSub].images.size();
    }
    return 0;
}

std::string MenuState::LevelLabel(Game& game, int idx) const
{
    const auto& catalog = game.GetResources().GetCatalog();
    switch (m_level)
    {
        case 0:
            if (idx >= 0 && idx < (int)catalog.size()) return catalog[idx].name;
            break;
        case 1:
            if (m_selCat >= 0 && m_selCat < (int)catalog.size() &&
                idx >= 0 && idx < (int)catalog[m_selCat].subcategories.size())
                return catalog[m_selCat].subcategories[idx].name;
            break;
        case 2:
            if (m_selCat >= 0 && m_selCat < (int)catalog.size() &&
                m_selSub >= 0 && m_selSub < (int)catalog[m_selCat].subcategories.size())
            {
                const auto& sub = catalog[m_selCat].subcategories[m_selSub];
                if (idx >= 0 && idx < (int)sub.images.size())
                    return sub.images[idx].fileName;
            }
            break;
    }
    return "";
}

// Map a screen point to a tile index on level 2 (-1 if outside the grid)
int MenuState::TileIndexAt(Vector2 mouse) const
{
    int gx = (int)((mouse.x - GridX()) / (TileW() + TileGap()));
    int gy = (int)((mouse.y - LIST_Y + m_scrollOffset) / (TileH() + TileGap()));
    if (gx < 0 || gx >= TILE_COLS || gy < 0) return -1;

    // Verify the point is inside the actual tile cell (not in the gap)
    float cx = GridX() + gx * (TileW() + TileGap());
    float cy = LIST_Y - m_scrollOffset + gy * (TileH() + TileGap());
    if (mouse.x < cx || mouse.x > cx + TileW()) return -1;
    if (mouse.y < cy || mouse.y > cy + TileH()) return -1;
    return gy * TILE_COLS + gx;
}

// Completion cache (0..1) for every image in the current subcategory
void MenuState::EnsureProgressCache(Game& game)
{
    if (!m_progress.empty()) return;
    const auto& catalog = game.GetResources().GetCatalog();
    if (m_selCat < 0 || m_selCat >= (int)catalog.size()) return;
    if (m_selSub < 0 || m_selSub >= (int)catalog[m_selCat].subcategories.size()) return;

    const auto& sub = catalog[m_selCat].subcategories[m_selSub];
    static const int grids[4][2] = { {5,5}, {8,8}, {16,16}, {32,32} };
    for (const auto& img : sub.images)
    {
        std::string path = game.GetResources().ResolvePath(img.relPath);
        float pct = 0.0f;
        for (int gi = 0; gi < 4; ++gi)
        {
            float elapsed = 0;
            int pc = 0, bc = 0;
            if (SaveManager::LoadHeader(path, grids[gi][0], grids[gi][1],
                                        elapsed, pc, bc) && pc > 1)
            {
                pct = std::clamp((float)(pc - bc) / (float)(pc - 1), 0.0f, 1.0f);
                break; // first grid with a save wins
            }
        }
        m_progress[img.relPath] = pct;
    }
}

void MenuState::HandleRowClick(Game& game, int idx, int tRows, int tCols)
{
    const auto& catalog = game.GetResources().GetCatalog();

    if (m_level == 0)
    {
        m_selCat = idx;
        m_level = 1;
        m_scrollOffset = 0;
        m_selectedIndex = -1;
        return;
    }

    if (m_level == 1)
    {
        m_selSub = idx;
        m_level = 2;
        m_scrollOffset = 0;
        m_selectedIndex = -1;
        m_progress.clear();
        if (m_preview.id != 0) { UnloadTexture(m_preview); m_preview = { 0 }; }
        m_previewKey.clear();
        return;
    }

    // Level 2 — select the image and load its preview
    m_selectedIndex = idx;
    const auto& sub = catalog[m_selCat].subcategories[m_selSub];
    LoadPreview(game, sub.images[idx].relPath);
}

void MenuState::LoadPreview(Game& game, const std::string& relPath)
{
    if (m_previewKey == relPath) return;
    if (m_preview.id != 0) UnloadTexture(m_preview);
    m_preview = { 0 };

    Image img = LoadImage(game.GetResources().ResolvePath(relPath).c_str());
    if (img.data)
    {
        int ms = 300;
        if (img.width > ms || img.height > ms)
        { float s = (float)ms / std::max(img.width, img.height); ImageResize(&img, (int)(img.width*s), (int)(img.height*s)); }
        m_preview = LoadTextureFromImage(img);
        SetTextureFilter(m_preview, TEXTURE_FILTER_BILINEAR);
        UnloadImage(img);
    }
    m_previewKey = relPath;
}

// Jump straight to the first image in the catalog that is not fully
// assembled (no save, or progress < 100%)
void MenuState::SelectFirstUnfinished(Game& game)
{
    const auto& catalog = game.GetResources().GetCatalog();
    static const int grids[4][2] = { {5,5}, {8,8}, {16,16}, {32,32} };

    for (int ci = 0; ci < (int)catalog.size(); ++ci)
    {
        for (int si = 0; si < (int)catalog[ci].subcategories.size(); ++si)
        {
            const auto& sub = catalog[ci].subcategories[si];
            for (int ii = 0; ii < (int)sub.images.size(); ++ii)
            {
                std::string path = game.GetResources().ResolvePath(sub.images[ii].relPath);
                bool unfinished = false;
                for (int gi = 0; gi < 4 && !unfinished; ++gi)
                {
                    float elapsed = 0;
                    int pc = 0, bc = 0;
                    if (SaveManager::LoadHeader(path, grids[gi][0], grids[gi][1],
                                                elapsed, pc, bc) && pc > 1)
                    {
                        float pct = std::clamp((float)(pc - bc) / (float)(pc - 1), 0.0f, 1.0f);
                        if (pct < 1.0f) unfinished = true;
                    }
                    else if (!SaveManager::HasSave(path, grids[gi][0], grids[gi][1]))
                    {
                        unfinished = true; // not started at this grid
                    }
                }

                if (unfinished)
                {
                    m_selCat = ci;
                    m_selSub = si;
                    m_selectedIndex = ii;
                    m_level = 2;
                    LoadPreview(game, sub.images[ii].relPath);
                    return;
                }
            }
        }
    }
}

// True if the image has a fully assembled save at any difficulty grid
bool MenuState::IsCompleted(const std::string& path) const
{
    static const int grids[4][2] = { {5,5}, {8,8}, {16,16}, {32,32} };
    for (int gi = 0; gi < 4; ++gi)
    {
        float elapsed = 0;
        int pc = 0, bc = 0;
        if (SaveManager::LoadHeader(path, grids[gi][0], grids[gi][1],
                                    elapsed, pc, bc) && pc > 1)
        {
            float pct = (float)(pc - bc) / (float)(pc - 1);
            if (pct >= 0.999f) return true;
        }
    }
    return false;
}

void MenuState::StartGame(Game& game, int tRows, int tCols)
{
    const auto& catalog = game.GetResources().GetCatalog();
    const auto& sub = catalog[m_selCat].subcategories[m_selSub];
    const std::string& rel = sub.images[m_selectedIndex].relPath;
    std::string path = game.GetResources().ResolvePath(rel);

    // Already fully assembled? Ask before re-assembling
    if (IsCompleted(path))
    {
        m_confirming = true;
        return;
    }

    game.selectedImagePath = path;
    game.selectedRows = tRows;
    game.selectedCols = tCols;
    game.continueGame = SaveManager::HasSave(path, tRows, tCols);
    game.ChangeState(std::make_unique<PlayState>());
}

void MenuState::Update(Game& game)
{
    Vector2 mouse = GetMousePosition();
    int sw = GetScreenWidth(), sh = GetScreenHeight();

    int preset = SettingsState::GetSettings().difficultyPreset;
    int tRows, tCols;
    GetDiffFromPreset(preset, tRows, tCols);

    // ---- Confirmation dialog blocks all other input ----
    if (m_confirming)
    {
        int cX = sw / 2, cY = sh / 2 - 40;
        float s = UiScale();
        Rectangle btnYes = { (float)(cX - 130 * s), (float)(cY + 50 * s), 120.0f * s, 44.0f * s };
        Rectangle btnNo  = { (float)(cX + 10 * s),  (float)(cY + 50 * s), 120.0f * s, 44.0f * s };
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            if (CheckCollisionPointRec(mouse, btnYes))
            {
                // Re-assemble: clear the completed saves and start fresh
                m_confirming = false;
                const auto& catalog = game.GetResources().GetCatalog();
                const auto& sub = catalog[m_selCat].subcategories[m_selSub];
                std::string path = game.GetResources().ResolvePath(sub.images[m_selectedIndex].relPath);
                static const int grids[4][2] = { {5,5}, {8,8}, {16,16}, {32,32} };
                for (int gi = 0; gi < 4; ++gi)
                    SaveManager::DeleteSave(path, grids[gi][0], grids[gi][1]);
                m_progress.clear();

                game.selectedImagePath = path;
                game.selectedRows = tRows;
                game.selectedCols = tCols;
                game.continueGame = false;
                game.ChangeState(std::make_unique<PlayState>());
                return;
            }
            if (CheckCollisionPointRec(mouse, btnNo))
            {
                m_confirming = false;
                return;
            }
        }
        return;
    }

    int rowCount = LevelCount(game);

    // Hover: list rows on levels 0/1, tiles on level 2
    m_hoveredIndex = -1;
    int listH = sh - LIST_Y - 30;
    bool inListX = (m_level == 2)
        ? (mouse.x >= GridX() - 6 && mouse.x <= GridX() + GridW() + 6)
        : (mouse.x >= LIST_X - 6 && mouse.x <= LIST_X + LIST_W + 6);
    bool inListY = (mouse.y >= LIST_Y - 6 && mouse.y <= LIST_Y - 6 + listH);

    if (m_level == 2)
    {
        int ti = TileIndexAt(mouse);
        if (ti >= 0 && ti < rowCount) m_hoveredIndex = ti;
    }
    else if (inListX && inListY)
    {
        int relY = (int)mouse.y - LIST_Y + m_scrollOffset;
        int idx = relY / ROW_H;
        if (idx >= 0 && idx < rowCount) m_hoveredIndex = idx;
    }

    // Click
    if (inListX && inListY && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && m_hoveredIndex >= 0)
    {
        HandleRowClick(game, m_hoveredIndex, tRows, tCols);
        return;
    }

    // Enter key to start (level 2 only)
    if (IsKeyPressed(KEY_ENTER) && m_level == 2 && m_selectedIndex >= 0)
    {
        StartGame(game, tRows, tCols);
        return;
    }

    // Scroll — by list rows on levels 0/1, by tile rows on level 2
    int wheel = GetMouseWheelMove();
    if (wheel != 0)
    {
        int step = (m_level == 2) ? (TileH() + TileGap()) * 3 : ROW_H * 3;
        m_scrollOffset -= wheel * step;
        int rowsVis = (m_level == 2) ? (int)std::ceil((double)rowCount / TILE_COLS) : rowCount;
        int unit = (m_level == 2) ? (TileH() + TileGap()) : ROW_H;
        int maxScroll = std::max(0, rowsVis * unit - sh + LIST_Y + 40);
        m_scrollOffset = std::clamp(m_scrollOffset, 0, maxScroll);
    }

    // Start button — overlay at the bottom of the preview (level 2 only)
    Rectangle pr = PreviewRect();
    float s = UiScale();
    Rectangle btnR = { (float)(PrevCenterX() - 100 * s), pr.y + pr.height - 62.0f * s, 200.0f * s, 46.0f * s };
    if (m_level == 2 && m_selectedIndex >= 0 &&
        CheckCollisionPointRec(mouse, btnR) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        StartGame(game, tRows, tCols);
        return;
    }

    // Back
    if (mouse.x >= 15 && mouse.x < 85 && mouse.y >= 15 && mouse.y < 45 &&
        IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        if (m_level > 0)
        {
            m_level--;
            m_scrollOffset = 0;
            m_selectedIndex = -1;
            if (m_level == 0) m_selCat = -1;
            if (m_preview.id != 0) { UnloadTexture(m_preview); m_preview = { 0 }; }
            m_previewKey.clear();
        }
        else
        {
            game.ChangeState(std::make_unique<MainMenuState>());
        }
    }
}

void MenuState::Draw(Game& game)
{
    int sh = GetScreenHeight(), sw = GetScreenWidth();
    Font& font = game.GetFont();

    DrawRectangle(0, 0, sw, sh, BG);

    int preset = SettingsState::GetSettings().difficultyPreset;
    int tRows, tCols;
    GetDiffFromPreset(preset, tRows, tCols);
    const char* diffNames[4] = { "25 шт (5x5)", "64 шт (8x8)", "256 шт (16x16)", "1024 шт (32x32)" };

    const auto& catalog = game.GetResources().GetCatalog();
    int rowCount = LevelCount(game);

    // Back
    DrawRectangle(12, 12, 75, 34, ColorAlpha(ACC, 0.10f));
    DrawRectangleLines(12, 12, 75, 34, ColorAlpha(ACC, 0.25f));
    DrawGameText(font, "\xE2\x86\x90 Назад", 22, 20, 16, ACC);

    // Title (breadcrumb for deeper levels)
    std::string title = "Выберите категорию";
    std::string subTitle = TextFormat("%d категорий  |  %s",
                                      (int)catalog.size(), diffNames[preset]);
    if (m_level >= 1 && m_selCat >= 0 && m_selCat < (int)catalog.size())
    {
        title = catalog[m_selCat].name + " — выберите подкатегорию";
        subTitle = TextFormat("%d подкатегорий", (int)catalog[m_selCat].subcategories.size());
    }
    if (m_level >= 2 && m_selCat >= 0 && m_selCat < (int)catalog.size() &&
        m_selSub >= 0 && m_selSub < (int)catalog[m_selCat].subcategories.size())
    {
        const auto& sub = catalog[m_selCat].subcategories[m_selSub];
        title = catalog[m_selCat].name + " / " + sub.name + " — выберите картинку";
        subTitle = TextFormat("%d картинок  |  %s", (int)sub.images.size(), diffNames[preset]);
    }

    DrawGameText(font, title.c_str(), LIST_X, 62, 26, TEXT);
    DrawGameText(font, subTitle.c_str(), LIST_X, 94, 13, MUT);

    int listH = sh - LIST_Y - 30;
    int panelX = (m_level == 2) ? GridX() : LIST_X;
    int panelW = (m_level == 2) ? GridW() : LIST_W;
    DrawRectangle(panelX - 6, LIST_Y - 6, panelW + 12, listH, CARD);
    DrawRectangleLines(panelX - 6, LIST_Y - 6, panelW + 12, listH, ColorAlpha(MUT, 0.18f));

    if (m_level == 2)
    {
        // ---------------- Tile grid ----------------
        EnsureProgressCache(game);
        const auto& sub = catalog[m_selCat].subcategories[m_selSub];

        int rowsVis = (int)std::ceil((double)rowCount / TILE_COLS);
        for (int r = 0; r < rowsVis; ++r)
        {
            for (int c = 0; c < TILE_COLS; ++c)
            {
                int idx = r * TILE_COLS + c;
                if (idx >= rowCount) break;

                int tx = GridX() + c * (TileW() + TileGap());
                int ty = LIST_Y + r * (TileH() + TileGap()) - m_scrollOffset;
                if (ty + TileH() < 0 || ty > sh) continue;

                Rectangle tile = { (float)tx, (float)ty, (float)TileW(), (float)TileH() };
                bool sel = (idx == m_selectedIndex);
                bool hov = (idx == m_hoveredIndex);

                DrawRectangleRounded(tile, 3, 1, hov ? HOV : CARD);

                // Thumbnail fitted into the tile
                Texture2D thumb = game.GetResources().LoadThumbnailPath(sub.images[idx].relPath, 80);
                if (thumb.id != 0)
                {
                    float tw = (float)thumb.width, th = (float)thumb.height;
                    float sc = std::min((TileW() - 10) / tw, (TileH() - 10) / th);
                    float dw = tw * sc, dh = th * sc;
                    float dx = tx + (TileW() - dw) / 2.0f;
                    float dy = ty + (TileH() - dh) / 2.0f;

                    // Base: semi-transparent (unfinished look)
                    DrawTexturePro(thumb,
                        Rectangle{ 0, 0, tw, th },
                        Rectangle{ dx, dy, dw, dh },
                        Vector2{ 0, 0 }, 0.0f, ColorAlpha(WHITE, 0.35f));

                    // Completion fill: bottom part becomes fully opaque
                    float pct = 0.0f;
                    auto it = m_progress.find(sub.images[idx].relPath);
                    if (it != m_progress.end()) pct = it->second;
                    if (pct > 0.01f)
                    {
                        float fh = dh * pct;
                        Rectangle src = { 0, th * (1.0f - pct), tw, th * pct };
                        Rectangle fdst = { dx, dy + dh - fh, dw, fh };
                        DrawTexturePro(thumb, src, fdst, Vector2{ 0, 0 }, 0.0f, WHITE);
                    }

                    // Small percent label at the bottom-right corner
                    if (pct > 0.01f)
                    {
                        DrawRectangle((int)(tx + TileW() - 34), ty + TileH() - 16, 30, 14,
                                      ColorAlpha(BG, 0.75f));
                        DrawGameText(font, TextFormat("%d%%", (int)(pct * 100.0f)),
                                     tx + TileW() - 30, ty + TileH() - 15, 11, ACC);
                    }
                }

                if (sel) DrawRectangleRoundedLines(tile, 3, 1, ACC);
                else if (hov) DrawRectangleRoundedLines(tile, 3, 1, ColorAlpha(ACC, 0.35f));
            }
        }
    }
    else
    {
        // ---------------- List (categories / subcategories) ----------------
        for (int i = 0; i < rowCount; ++i)
        {
            int ry = LIST_Y + i * ROW_H - m_scrollOffset;
            if (ry + ROW_H < 0 || ry > sh) continue;

            bool hov = (i == m_hoveredIndex);
            Color rowBg = hov ? HOV : CARD;
            DrawRectangle(LIST_X, ry, LIST_W, ROW_H, rowBg);
            if (!hov) DrawLine(LIST_X, ry + ROW_H, LIST_X + LIST_W, ry + ROW_H, ColorAlpha(MUT, 0.06f));

            DrawGameText(font, "\xE2\x86\x92", LIST_X + 12, ry + 11, 14, ACC);
            std::string label = LevelLabel(game, i);
            DrawGameText(font, label.c_str(), LIST_X + 34, ry + 12, 14, TEXT);
        }
    }

    // ---- Preview + Start button (right half, image level only) ----
    if (m_level == 2)
    {
        Rectangle pr = PreviewRect();
        int px = (int)pr.x, py = (int)pr.y, pz = (int)pr.width;

        if (m_preview.id != 0)
        {
            // Draw the image directly on the background — no card underneath
            float sc = (float)(pz - 12) / std::max((float)m_preview.width, (float)m_preview.height);
            float dw = m_preview.width * sc, dh = m_preview.height * sc;
            float dx = px + (pz - dw) / 2.0f;
            float dy = py + (pz - dh) / 2.0f;

            // Soft drop shadow (single layer, sharp corners like the preview)
            DrawRectangle((int)(dx + 4), (int)(dy + 6), (int)dw, (int)dh,
                          Color{ 0, 0, 0, 60 });

            DrawTexturePro(m_preview, {0,0,(float)m_preview.width,(float)m_preview.height},
                           {dx, dy, dw, dh}, {0,0}, 0, WHITE);
        }
        else if (m_selectedIndex >= 0)
        {
            DrawGameText(font, "Превью", PrevCenterX() - 40, py + pz / 2 - 10, 18, MUT);
        }

        // "Начать" button overlaid at the bottom of the preview
        bool canStart = (m_selectedIndex >= 0);
        Vector2 mouse = GetMousePosition();
        float s = UiScale();
        Rectangle btnRR = { (float)(PrevCenterX() - 100 * s), pr.y + pr.height - 62.0f * s, 200.0f * s, 46.0f * s };
        bool hoverBtn = CheckCollisionPointRec(mouse, btnRR) && canStart;
        Color btnBg = canStart ? (hoverBtn ? Color{ 0, 0, 0, 190 } : Color{ 0, 0, 0, 150 })
                               : Color{ 0, 0, 0, 80 };
        Color btnBorder = canStart ? (hoverBtn ? ACC : ColorAlpha(WHITE, 0.6f))
                                   : ColorAlpha(WHITE, 0.2f);
        Color btnText = canStart ? WHITE : ColorAlpha(WHITE, 0.4f);
        DrawRectangleRounded(btnRR, 6, 1, btnBg);
        DrawRectangleRoundedLines(btnRR, 6, 1, btnBorder);
        const char* startLabel = "Начать";
        DrawGameText(font, startLabel,
                     PrevCenterX() - (int)MeasureGameText(font, startLabel, (int)(18 * s)) / 2,
                     (int)btnRR.y + (int)(14 * s), (int)(18 * s), btnText);
    }

    // Instructions
    DrawGameText(font, "Выберите картинку  |  «Начать» или Enter", 60, sh-28, 13, MUT);

    // ---- Re-assemble confirmation dialog ----
    if (m_confirming)
    {
        DrawRectangle(0, 0, sw, sh, Color{ 0, 0, 0, 120 });
        int cX = sw / 2, cY = sh / 2 - 40;
        float s = UiScale();
        Rectangle win = { (float)(cX - 260 * s), (float)(cY - 70 * s), 520.0f * s, 190.0f * s };
        DrawRectangleRounded(win, 6, 1, CARD);
        DrawRectangleRoundedLines(win, 6, 1, ColorAlpha(MUT, 0.3f));
        DrawGameText(font, "Вы уверены, что хотите",
                     cX - (int)(130 * s), cY - (int)(32 * s), (int)(20 * s), TEXT);
        DrawGameText(font, "повторно собрать эту картинку?",
                     cX - (int)(160 * s), cY + (int)(2 * s), (int)(20 * s), TEXT);

        Vector2 mouse = GetMousePosition();
        Rectangle btnYes = { (float)(cX - 130 * s), (float)(cY + 52 * s), 120.0f * s, 44.0f * s };
        Rectangle btnNo  = { (float)(cX + 10 * s),  (float)(cY + 52 * s), 120.0f * s, 44.0f * s };
        bool hy = CheckCollisionPointRec(mouse, btnYes);
        bool hn = CheckCollisionPointRec(mouse, btnNo);

        DrawRectangleRounded(btnYes, 6, 1, hy ? ColorAlpha(ACC, 0.18f) : ColorAlpha(ACC, 0.08f));
        DrawRectangleRoundedLines(btnYes, 6, 1, hy ? ACC : ColorAlpha(ACC, 0.35f));
        DrawGameText(font, "Да",
                     cX - (int)(130 * s) + (int)((120 * s - MeasureGameText(font, "Да", (int)(18 * s))) / 2),
                     cY + (int)(65 * s), (int)(18 * s), ACC);

        DrawRectangleRounded(btnNo, 6, 1, hn ? Color{ 0, 0, 0, 26 } : CARD);
        DrawRectangleRoundedLines(btnNo, 6, 1, hn ? ColorAlpha(TEXT, 0.6f) : ColorAlpha(MUT, 0.35f));
        DrawGameText(font, "Нет",
                     cX + (int)(10 * s) + (int)((120 * s - MeasureGameText(font, "Нет", (int)(18 * s))) / 2),
                     cY + (int)(65 * s), (int)(18 * s), TEXT);
    }
}
