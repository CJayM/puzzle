#include "states/MenuState.h"
#include "states/PlayState.h"
#include "states/SettingsState.h"
#include "states/MainMenuState.h"
#include "core/Game.h"
#include "core/Config.h"
#include "core/DrawHelper.h"
#include "core/SaveManager.h"
#include "ui/Button.h"
#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

// ---- Virtual pixels (dp) ----
// All sizes/positions below are in device-independent pixels; 1 logical
// unit = dpiScale physical px * user scale, so UI is the same physical
// size on any phone/tablet. Scale helpers live in ui:: (see ui/Button.h).

// List (categories / subcategories) layout
static const int ROW_H = 42;
static int ListW() { return std::min(400, (int)ui::LogW() - 32); }
static int ListX() { return 16; }
static const int LIST_Y = 120;

// Tile grid (image level) — left half of the window, fixed dp tile size,
// adaptive column count so the grid always fits the screen
static int TileW()   { return 128; }
static int TileH()   { return TileW(); }
static int TileGap() { return 6; }
static int GridX()   { return 16; }
static int TileCols(){ return std::max(2, (int)((ui::LogW() * 0.58f) / (TileW() + TileGap()))); }
static int GridW()   { return TileCols() * TileW() + (TileCols() - 1) * TileGap(); }

// Right half: big preview + "Начать" button overlaid on top
static int PrevCenterX() { return (int)(ui::LogW() * 3 / 4); }
static int PrevMax()     { return (int)std::min(ui::LogW() * 7 / 16.0f, ui::LogH() - 190.0f); }

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
    int sh = (int)ui::LogH();
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
    m_gridReady = false;
    m_tiles.clear();

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

// Build the thumbnail tile buttons for the current subcategory (level 2)
void MenuState::BuildGrid(Game& game)
{
    const auto& catalog = game.GetResources().GetCatalog();
    if (m_selCat < 0 || m_selCat >= (int)catalog.size()) return;
    if (m_selSub < 0 || m_selSub >= (int)catalog[m_selCat].subcategories.size()) return;

    m_tiles.clear();
    m_grid = ui::FlowLayout();
    m_grid.SetSpacing(TileGap());

    int count = (int)catalog[m_selCat].subcategories[m_selSub].images.size();
    // Reserve capacity: the grid stores pointers to m_tiles elements, so the
    // vector must not reallocate after AddItem() has recorded their addresses
    m_tiles.reserve((size_t)count);
    for (int i = 0; i < count; ++i)
    {
        m_tiles.emplace_back(0, 0, TileW(), TileH(), "");
        ui::Button& b = m_tiles.back();
        b.SetRadius(0);          // square tiles, no rounded corners
        // No border/hover ring from the button itself — selection and hover
        // outlines are drawn by MenuState (clipped to the panel)
        b.SetColors(Color{ 255, 255, 255, 255 }, ColorAlpha(MUT, 0.0f),
                    Color{ 40, 40, 45, 255 });
        b.SetHoverColors(HOV, ColorAlpha(ACC, 0.0f), Color{ 40, 40, 45, 255 });
        m_grid.AddItem(&b);
    }

    m_panel.Attach(&m_grid);
    m_gridReady = true;
    m_panel.SetScroll(0);
}

// Refresh tile textures and progress fill (every frame on level 2)
void MenuState::RefreshGrid(Game& game)
{
    if (!m_gridReady) return;
    const auto& catalog = game.GetResources().GetCatalog();
    const auto& sub = catalog[m_selCat].subcategories[m_selSub];

    for (size_t i = 0; i < m_tiles.size(); ++i)
    {
        m_tiles[i].SetTexture(
            game.GetResources().LoadThumbnailPath(sub.images[i].relPath, 128));
        float pct = 0.0f;
        auto it = m_progress.find(sub.images[i].relPath);
        if (it != m_progress.end()) pct = it->second;
        m_tiles[i].SetFill(pct);
    }
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
        BuildGrid(game);
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
                    BuildGrid(game);
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

    int preset = SettingsState::GetSettings().difficultyPreset;
    int tRows, tCols;
    GetDiffFromPreset(preset, tRows, tCols);

    // ---- Confirmation dialog blocks all other input ----
    if (m_confirming)
    {
        const float dl = ui::LogW() / 2, dc = ui::LogH() / 2 - 40;
        Font& dialogFont = game.GetFont();

        ui::Button btnYes(0, 0, 0, 0, "Да");
        ui::Button btnNo(0, 0, 0, 0, "Нет");
        btnYes.UpdateAutoSize(dialogFont);
        btnNo.UpdateAutoSize(dialogFont);
        btnYes.SetRect(dl - btnYes.MinWidth() - 6, dc + 52, 0, 0);
        btnNo.SetRect(dl + 6, dc + 52, 0, 0);

        if (btnYes.Clicked())
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

        if (btnNo.Clicked())
        {
            m_confirming = false;
            return;
        }
        return;
    }

    int rowCount = LevelCount(game);

    // ---- Thumbnail panel refresh (level 2) ----
    if (m_level == 2)
    {
        EnsureProgressCache(game);
        if (!m_gridReady) BuildGrid(game);
        m_panel.SetRect(GridX(), LIST_Y, GridW(),
                        (int)ui::LogH() - LIST_Y - 40);
        RefreshGrid(game);
        m_panel.ComputeChildren();
        m_panel.Update();   // wheel + scrollbar dragging
    }

    // Hover: list rows on levels 0/1, tiles on level 2
    m_hoveredIndex = -1;
    int listH = (int)ui::LogH() - LIST_Y - 30;
    bool inListX = (m_level == 2)
        ? (mouse.x >= GridX() - 6 && mouse.x <= GridX() + GridW() + 6)
        : (mouse.x >= ListX() - 6 && mouse.x <= ListX() + ListW() + 6);
    bool inListY = (mouse.y >= LIST_Y - 6 && mouse.y <= LIST_Y - 6 + listH);

    if (m_level == 2)
    {
        for (size_t i = 0; i < m_tiles.size(); ++i)
            if (m_tiles[i].Hovered()) { m_hoveredIndex = (int)i; break; }
    }
    else if (inListX && inListY)
    {
        int relY = (int)mouse.y - LIST_Y + m_scrollOffset;
        int idx = relY / ROW_H;
        if (idx >= 0 && idx < rowCount) m_hoveredIndex = idx;
    }

    // Click on a tile (level 2) or a list row (levels 0/1)
    if (m_level == 2)
    {
        // Panel drag-to-scroll: a press inside the panel starts a drag; a
        // short press without movement on a tile selects it (release)
        bool inPanel = CheckCollisionPointRec(mouse, m_panel.Rect());

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            // Don't start a panel drag when grabbing the scrollbar thumb
            m_panelDragging = inPanel && !m_panel.OnThumb(mouse);
            if (m_panelDragging)
            {
                m_panelDragStartY = mouse.y;
                m_panelDragStartScroll = m_panel.GetScroll();
                m_panelDragMoved = false;
                m_panelPressTile = -1;
                for (size_t i = 0; i < m_tiles.size(); ++i)
                    if (m_tiles[i].Hovered()) { m_panelPressTile = (int)i; break; }
            }
        }
        else if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && m_panelDragging)
        {
            float delta = m_panelDragStartY - mouse.y; // physical px
            float threshold = 5.0f * ui::UiScale();
            if (fabsf(delta) > threshold)
            {
                m_panelDragMoved = true;
                m_panel.SetScroll(m_panelDragStartScroll + delta / ui::UiScale());
            }
        }
        else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && m_panelDragging)
        {
            m_panelDragging = false;
            if (!m_panelDragMoved && m_panelPressTile >= 0)
            {
                HandleRowClick(game, m_panelPressTile, tRows, tCols);
                return;
            }
        }
    }
    else if (inListX && inListY && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && m_hoveredIndex >= 0)
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

    // Scroll — list rows on levels 0/1; the panel handles its own scrollbar
    // and wheel on level 2
    if (m_level != 2)
    {
        int wheel = GetMouseWheelMove();
        if (wheel != 0)
        {
            m_scrollOffset -= wheel * ROW_H * 3;
            int maxScroll = std::max(0, rowCount * ROW_H - (int)ui::LogH() + LIST_Y + 40);
            m_scrollOffset = std::clamp(m_scrollOffset, 0, maxScroll);
        }
    }

    // Start button — overlay at the bottom of the preview (level 2 only)
    Rectangle pr = PreviewRect();
    ui::Button btnStart(0, 0, 0, 0, "Начать");
    btnStart.UpdateAutoSize(game.GetFont());
    btnStart.SetRect(PrevCenterX() - btnStart.MinWidth() / 2,
                     pr.y + pr.height - btnStart.MinHeight() - 16, 0, 0);
    btnStart.SetEnabled(m_level == 2 && m_selectedIndex >= 0);
    if (btnStart.Clicked())
    {
        StartGame(game, tRows, tCols);
        return;
    }

    // Back
    float bu = ui::UiScale();
    if (mouse.x >= 15 * bu && mouse.x < 85 * bu && mouse.y >= 15 * bu && mouse.y < 45 * bu &&
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
    float u = ui::UiScale();
    DrawRectangle((int)(12 * u), (int)(12 * u), (int)(75 * u), (int)(34 * u), ColorAlpha(ACC, 0.10f));
    DrawRectangleLines((int)(12 * u), (int)(12 * u), (int)(75 * u), (int)(34 * u), ColorAlpha(ACC, 0.25f));
    DrawGameText(font, "Назад", (int) (22 * u), (int) (20 * u), (int) (16 * u), ACC);

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

    DrawGameText(font, title.c_str(), ListX(), 62, (int)(26 * ui::UiScale()), TEXT);
    DrawGameText(font, subTitle.c_str(), ListX(), 94, (int) (16 * ui::UiScale()), MUT);

    // Panel background for the plain list (levels 0/1); the thumbnail
    // panel (level 2) draws its own background and scrollbar
    if (m_level != 2)
    {
        int listH = (int)ui::LogH() - LIST_Y - 30;
        DrawRectangle(ListX() - 6, LIST_Y - 6, ListW() + 12, listH, CARD);
        DrawRectangleLines(ListX() - 6, LIST_Y - 6, ListW() + 12, listH, ColorAlpha(MUT, 0.18f));
    }

    if (m_level == 2)
    {
        // ---------------- Thumbnail grid (scrollable panel) ----------------
        // Panel state (scroll, textures) is refreshed in Update(); draw only
        m_panel.Draw(font);

        // Selection / hover outlines (drawn on top of the panel, clipped to
        // its bounds so edge tiles don't draw outside). Tile rects already
        // include the scroll offset from Panel::ComputeChildren().
        Rectangle pr = m_panel.Rect();
        BeginScissorMode((int)pr.x, (int)pr.y, (int)pr.width, (int)pr.height);
        for (size_t i = 0; i < m_tiles.size(); ++i)
        {
            Rectangle r = m_tiles[i].Rect();
            if (i == (size_t)m_selectedIndex)
                DrawRectangleLinesEx(r, 2, ACC);
            else if ((int)i == m_hoveredIndex)
                DrawRectangleLinesEx(r, 2, ColorAlpha(ACC, 0.45f));
        }
        EndScissorMode();
    }
    else
    {
        // ---------------- List (categories / subcategories) ----------------
        for (int i = 0; i < rowCount; ++i)
        {
            int ry = LIST_Y + i * ROW_H - m_scrollOffset;
            if (ry + ROW_H < 0 || ry > ui::LogH()) continue;

            bool hov = (i == m_hoveredIndex);
            Color rowBg = hov ? HOV : CARD;
            DrawRectangle(ListX(), ry, ListW(), ROW_H, rowBg);
            if (!hov) DrawLine(ListX(), ry + ROW_H, ListX() + ListW(), ry + ROW_H, ColorAlpha(MUT, 0.06f));

            DrawGameText(font,
                         "\xE2\x86\x92",
                         ListX() + 12,
                         ry + 11,
                         (int) (20 * ui::UiScale()),
                         ACC);
            std::string label = LevelLabel(game, i);
            DrawGameText(font,
                         label.c_str(),
                         ListX() + 44,
                         ry + 12,
                         (int) (20 * ui::UiScale()),
                         TEXT);
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
            DrawGameText(font, "Превью", PrevCenterX() - 40, py + pz / 2 - 10, (int)(18 * ui::UiScale()), MUT);
        }

        // "Начать" button overlaid at the bottom of the preview
        ui::Button b(0, 0, 0, 0, "Начать");
        b.UpdateAutoSize(font);
        b.SetRect(PrevCenterX() - b.MinWidth() / 2,
                  pr.y + pr.height - b.MinHeight() - 16, 0, 0);
        b.SetEnabled(m_selectedIndex >= 0);
        b.SetFontSize(18);
        b.SetColors(Color{ 0, 0, 0, 150 }, ColorAlpha(WHITE, 0.6f), WHITE);
        b.SetHoverColors(Color{ 0, 0, 0, 190 }, ACC, WHITE);
        b.SetDisabledColors(Color{ 0, 0, 0, 80 }, ColorAlpha(WHITE, 0.2f), ColorAlpha(WHITE, 0.4f));
        b.Draw(font);
    }

    // ---- Re-assemble confirmation dialog ----
    if (m_confirming)
    {
        DrawRectangle(0, 0, sw, sh, Color{ 0, 0, 0, 120 });
        const float dl = ui::LogW() / 2, dc = ui::LogH() / 2 - 40;
        float s = ui::UiScale();
        Rectangle win = { (float)((dl - 260) * s), (float)((dc - 70) * s), 520.0f * s, 190.0f * s };
        DrawRectangleRounded(win, 6, 1, CARD);
        DrawRectangleRoundedLines(win, 6, 1, ColorAlpha(MUT, 0.3f));
        DrawGameText(font, "Вы уверены, что хотите",
                     (int)((dl - 130) * s), (int)((dc - 32) * s), (int)(20 * s), TEXT);
        DrawGameText(font, "повторно собрать эту картинку?",
                     (int)((dl - 160) * s), (int)((dc + 2) * s), (int)(20 * s), TEXT);

        // Yes / No buttons (auto-sized)
        ui::Button btnYes(0, 0, 0, 0, "Да");
        ui::Button btnNo(0, 0, 0, 0, "Нет");
        btnYes.UpdateAutoSize(font);
        btnNo.UpdateAutoSize(font);
        btnYes.SetRect(dl - btnYes.MinWidth() - 6, dc + 52, 0, 0);
        btnNo.SetRect(dl + 6, dc + 52, 0, 0);

        btnYes.SetColors(ColorAlpha(ACC, 0.08f), ColorAlpha(ACC, 0.35f), ACC);
        btnYes.SetHoverColors(ColorAlpha(ACC, 0.18f), ACC, ACC);
        btnYes.Draw(font);

        btnNo.SetColors(CARD, ColorAlpha(MUT, 0.35f), TEXT);
        btnNo.SetHoverColors(Color{ 0, 0, 0, 26 }, ColorAlpha(TEXT, 0.6f), TEXT);
        btnNo.Draw(font);
    }
}
