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

// Tile grid (image level) — left half of the window
static const int TILE_W = 96;
static const int TILE_H = 96;
static const int TILE_GAP = 6;
static const int TILE_COLS = 6;
static const int GRID_X = 16;
static const int GRID_W = TILE_COLS * TILE_W + (TILE_COLS - 1) * TILE_GAP; // 606

// Right half: big preview + "Начать" button overlaid on top
static const int PREV_MAX = 560;

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
static Rectangle PreviewRect()
{
    int sh = GetScreenHeight();
    int pz = std::min(PREV_MAX, sh - 190);
    int px = PrevCenterX() - pz / 2;
    int py = (sh - pz) / 2 - 10;
    return { (float)px, (float)py, (float)pz, (float)pz };
}

void MenuState::Enter(Game&)
{
    m_level = 0;
    m_selCat = -1;
    m_selSub = -1;
    m_selectedIndex = -1;
    m_scrollOffset = 0;
    m_preview = { 0 };
    m_previewKey.clear();
    m_progress.clear();
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
    int gx = (int)((mouse.x - GRID_X) / (TILE_W + TILE_GAP));
    int gy = (int)((mouse.y - LIST_Y + m_scrollOffset) / (TILE_H + TILE_GAP));
    if (gx < 0 || gx >= TILE_COLS || gy < 0) return -1;

    // Verify the point is inside the actual tile cell (not in the gap)
    float cx = GRID_X + gx * (TILE_W + TILE_GAP);
    float cy = LIST_Y - m_scrollOffset + gy * (TILE_H + TILE_GAP);
    if (mouse.x < cx || mouse.x > cx + TILE_W) return -1;
    if (mouse.y < cy || mouse.y > cy + TILE_H) return -1;
    return gy * TILE_COLS + gx;
}

// Completion cache (0..1) for every image in the current subcategory
void MenuState::EnsureProgressCache(Game& game, int tRows, int tCols)
{
    if (!m_progress.empty()) return;
    const auto& catalog = game.GetResources().GetCatalog();
    if (m_selCat < 0 || m_selCat >= (int)catalog.size()) return;
    if (m_selSub < 0 || m_selSub >= (int)catalog[m_selCat].subcategories.size()) return;

    const auto& sub = catalog[m_selCat].subcategories[m_selSub];
    for (const auto& img : sub.images)
    {
        std::string path = game.GetResources().ResolvePath(img.relPath);
        float elapsed = 0;
        int pc = 0, bc = 0;
        float pct = 0.0f;
        if (SaveManager::LoadHeader(path, tRows, tCols, elapsed, pc, bc) && pc > 1)
        {
            pct = (float)(pc - bc) / (float)(pc - 1);
            pct = std::clamp(pct, 0.0f, 1.0f);
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
    const std::string& rel = sub.images[idx].relPath;
    if (m_previewKey != rel)
    {
        if (m_preview.id != 0) UnloadTexture(m_preview);
        m_preview = { 0 };
        Image img = LoadImage(game.GetResources().ResolvePath(rel).c_str());
        if (img.data)
        {
            int ms = 300;
            if (img.width > ms || img.height > ms)
            { float s = (float)ms / std::max(img.width, img.height); ImageResize(&img, (int)(img.width*s), (int)(img.height*s)); }
            m_preview = LoadTextureFromImage(img);
            SetTextureFilter(m_preview, TEXTURE_FILTER_BILINEAR);
            UnloadImage(img);
        }
        m_previewKey = rel;
    }
}

void MenuState::StartGame(Game& game, int tRows, int tCols)
{
    const auto& catalog = game.GetResources().GetCatalog();
    const auto& sub = catalog[m_selCat].subcategories[m_selSub];
    const std::string& rel = sub.images[m_selectedIndex].relPath;

    game.selectedImagePath = game.GetResources().ResolvePath(rel);
    game.selectedRows = tRows;
    game.selectedCols = tCols;
    game.continueGame = SaveManager::HasSave(game.selectedImagePath, tRows, tCols);
    game.ChangeState(std::make_unique<PlayState>());
}

void MenuState::Update(Game& game)
{
    Vector2 mouse = GetMousePosition();
    int sw = GetScreenWidth(), sh = GetScreenHeight();

    int preset = SettingsState::GetSettings().difficultyPreset;
    int tRows, tCols;
    GetDiffFromPreset(preset, tRows, tCols);

    int rowCount = LevelCount(game);

    // Hover: list rows on levels 0/1, tiles on level 2
    m_hoveredIndex = -1;
    int listH = sh - LIST_Y - 30;
    bool inListX = (m_level == 2)
        ? (mouse.x >= GRID_X - 6 && mouse.x <= GRID_X + GRID_W + 6)
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
        int step = (m_level == 2) ? (TILE_H + TILE_GAP) * 3 : ROW_H * 3;
        m_scrollOffset -= wheel * step;
        int rowsVis = (m_level == 2) ? (int)std::ceil((double)rowCount / TILE_COLS) : rowCount;
        int unit = (m_level == 2) ? (TILE_H + TILE_GAP) : ROW_H;
        int maxScroll = std::max(0, rowsVis * unit - sh + LIST_Y + 40);
        m_scrollOffset = std::clamp(m_scrollOffset, 0, maxScroll);
    }

    // Start button — overlay at the bottom of the preview (level 2 only)
    Rectangle pr = PreviewRect();
    Rectangle btnR = { (float)(PrevCenterX() - 100), pr.y + pr.height - 62.0f, 200.0f, 46.0f };
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
    int panelX = (m_level == 2) ? GRID_X : LIST_X;
    int panelW = (m_level == 2) ? GRID_W : LIST_W;
    DrawRectangle(panelX - 6, LIST_Y - 6, panelW + 12, listH, CARD);
    DrawRectangleLines(panelX - 6, LIST_Y - 6, panelW + 12, listH, ColorAlpha(MUT, 0.18f));

    if (m_level == 2)
    {
        // ---------------- Tile grid ----------------
        EnsureProgressCache(game, tRows, tCols);
        const auto& sub = catalog[m_selCat].subcategories[m_selSub];

        int rowsVis = (int)std::ceil((double)rowCount / TILE_COLS);
        for (int r = 0; r < rowsVis; ++r)
        {
            for (int c = 0; c < TILE_COLS; ++c)
            {
                int idx = r * TILE_COLS + c;
                if (idx >= rowCount) break;

                int tx = GRID_X + c * (TILE_W + TILE_GAP);
                int ty = LIST_Y + r * (TILE_H + TILE_GAP) - m_scrollOffset;
                if (ty + TILE_H < 0 || ty > sh) continue;

                Rectangle tile = { (float)tx, (float)ty, (float)TILE_W, (float)TILE_H };
                bool sel = (idx == m_selectedIndex);
                bool hov = (idx == m_hoveredIndex);

                DrawRectangleRounded(tile, 3, 1, hov ? HOV : CARD);

                // Thumbnail fitted into the tile
                Texture2D thumb = game.GetResources().LoadThumbnailPath(sub.images[idx].relPath, 80);
                if (thumb.id != 0)
                {
                    float tw = (float)thumb.width, th = (float)thumb.height;
                    float sc = std::min((TILE_W - 10) / tw, (TILE_H - 10) / th);
                    float dw = tw * sc, dh = th * sc;
                    float dx = tx + (TILE_W - dw) / 2.0f;
                    float dy = ty + (TILE_H - dh) / 2.0f;

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
                        DrawRectangle((int)(tx + TILE_W - 34), ty + TILE_H - 16, 30, 14,
                                      ColorAlpha(BG, 0.75f));
                        DrawGameText(font, TextFormat("%d%%", (int)(pct * 100.0f)),
                                     tx + TILE_W - 30, ty + TILE_H - 15, 11, ACC);
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

            DrawGameText(font, "\xE2\x96\xB8", LIST_X + 12, ry + 11, 14, ACC);
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
        Rectangle btnRR = { (float)(PrevCenterX() - 100), pr.y + pr.height - 62.0f, 200.0f, 46.0f };
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
                     PrevCenterX() - (int)MeasureGameText(font, startLabel, 18) / 2,
                     (int)btnRR.y + 14, 18, btnText);
    }

    // Instructions
    DrawGameText(font, "Выберите картинку  |  «Начать» или Enter", 60, sh-28, 13, MUT);
}
