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

// Section header inside the image panel (subcategory name)
static const int HEADER_H = 34;

// Tile grid (image level) — left half of the window, fixed dp tile size,
// adaptive column count so the grid always fits the screen
static int TileW()   { return 128; }
static int TileH()   { return TileW(); }
static int TileGap() { return 6; }
static int GridX()   { return 16; }
static int TileCols(){ return std::max(2, (int)((ui::LogW() * 0.58f) / (TileW() + TileGap()))); }
static int GridW()   { return TileCols() * TileW() + (TileCols() - 1) * TileGap(); }

// Right half: big preview — confined to the area right of the thumbnail
// panel (never overlaps it), centered in that remaining space
static int GridRight()   { return GridX() + GridW(); }
static int PrevCenterX() { return (int)((GridRight() + ui::LogW()) / 2.0f); }
static int PrevMax()     { return (int)std::max(0.0f, std::min(ui::LogW() - GridRight() - 40.0f,
                                                ui::LogH() - 190.0f)); }

static const Color BG   = { 240, 242, 245, 255 };
static const Color CARD = { 255, 255, 255, 255 };
static const Color TEXT = { 40, 40, 45, 255 };
static const Color MUT  = { 150, 150, 155, 255 };
static const Color ACC  = { 55, 110, 210, 255 };
static const Color ACCL = { 220, 235, 255, 255 };
static const Color HOV  = { 235, 240, 250, 255 };
static const Color HDR  = { 70, 80, 95, 255 };

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
    m_selectedIndex = -1;
    m_scrollOffset = 0;
    m_preview = { 0 };
    m_previewKey.clear();
    m_progress.clear();
    ResetImagePanel();

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

// Number of rows in the category list (level 0)
int MenuState::LevelCount(Game& game) const
{
    return (int)game.GetResources().GetCatalog().size();
}

std::string MenuState::LevelLabel(Game& game, int idx) const
{
    const auto& catalog = game.GetResources().GetCatalog();
    if (idx < 0 || idx >= (int)catalog.size()) return "";
    return catalog[idx].name;
}

// Drop the image panel (back to the category list)
void MenuState::ResetImagePanel()
{
    m_grid = ui::SectionedFlowLayout();
    m_tiles.clear();
    m_headers.clear();
    m_tileRefs.clear();
    m_panel.Attach(&m_grid);
    m_panel.SetScroll(0);
    m_gridReady = false;
}

// Build the image panel of a category: for every subcategory a header row
// followed by its thumbnails, all inside one scrollable flow
void MenuState::BuildImagePanel(Game& game)
{
    ResetImagePanel();

    const auto& catalog = game.GetResources().GetCatalog();
    if (m_selCat < 0 || m_selCat >= (int)catalog.size()) return;
    const auto& cat = catalog[m_selCat];

    m_grid.SetSpacing(TileGap());
    m_grid.SetHeaderHeight(HEADER_H);

    size_t tileCount = 0, groupCount = 0;
    for (const auto& sub : cat.subcategories)
    {
        tileCount += sub.images.size();
        if (!sub.images.empty()) ++groupCount;
    }

    // The layout keeps pointers into these vectors — reserve before adding
    m_tiles.reserve(tileCount);
    m_headers.reserve(groupCount);
    m_tileRefs.reserve(tileCount);

    std::vector<int> groups;
    groups.reserve(groupCount);

    for (int si = 0; si < (int)cat.subcategories.size(); ++si)
    {
        const auto& sub = cat.subcategories[si];
        if (sub.images.empty()) continue;

        m_headers.emplace_back(
            TextFormat("%s (%d)", sub.name.c_str(), (int)sub.images.size()),
            20.0f, HDR);
        m_grid.AddItem(&m_headers.back());

        for (int ii = 0; ii < (int)sub.images.size(); ++ii)
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
            m_tileRefs.push_back({ si, ii });
        }

        groups.push_back((int)sub.images.size());
    }

    m_grid.SetGroups(std::move(groups));
    m_gridReady = true;
}

// Refresh textures and progress fill of the tiles inside the panel (every
// frame). Thumbnails are decoded lazily, so loading a 200-image category all
// at once would freeze the first frame — only visible tiles are requested.
void MenuState::RefreshTiles(Game& game)
{
    if (!m_gridReady) return;

    const auto& catalog = game.GetResources().GetCatalog();
    if (m_selCat < 0 || m_selCat >= (int)catalog.size()) return;
    const auto& cat = catalog[m_selCat];

    const Rectangle panelR = m_panel.Rect();

    for (size_t i = 0; i < m_tiles.size() && i < m_tileRefs.size(); ++i)
    {
        const auto& ref = m_tileRefs[i];
        if (ref.first >= (int)cat.subcategories.size()) continue;
        const auto& sub = cat.subcategories[ref.first];
        if (ref.second >= (int)sub.images.size()) continue;
        const auto& img = sub.images[ref.second];

        const Rectangle tileR = m_tiles[i].Rect();
        const bool visible = tileR.y < panelR.y + panelR.height + 4 &&
                             tileR.y + tileR.height > panelR.y - 4;

        if (visible)
            m_tiles[i].SetTexture(game.GetResources().LoadThumbnailPath(img.relPath, 128));

        float pct = 0.0f;
        auto it = m_progress.find(img.relPath);
        if (it != m_progress.end())
            pct = it->second;
        else if (visible)
        {
            pct = LoadProgress(game, img.relPath);
            m_progress[img.relPath] = pct;
        }
        m_tiles[i].SetFill(pct);
    }
}

// Completion (0..1) of one image at the highest difficulty that has a save
float MenuState::LoadProgress(Game& game, const std::string& relPath)
{
    static const int grids[4][2] = { {5,5}, {8,8}, {16,16}, {32,32} };
    std::string path = game.GetResources().ResolvePath(relPath);
    for (int gi = 0; gi < 4; ++gi)
    {
        float elapsed = 0;
        int pc = 0, bc = 0;
        if (SaveManager::LoadHeader(path, grids[gi][0], grids[gi][1],
                                    elapsed, pc, bc) && pc > 1)
            return std::clamp((float)(pc - bc) / (float)(pc - 1), 0.0f, 1.0f);
    }
    return 0.0f;
}

// Assign the panel rect and lay its content out with the current scroll
void MenuState::LayoutPanel()
{
    m_panel.SetRect(GridX(), LIST_Y, GridW(),
                    (int)ui::LogH() - LIST_Y - 40);
    m_panel.ComputeChildren();
}

// Scroll the panel so the given tile is centered vertically
void MenuState::ScrollToTile(int idx)
{
    if (!m_gridReady || idx < 0 || idx >= (int)m_tiles.size()) return;
    LayoutPanel(); // the panel size and content height must be known first
    float target = m_tiles[idx].Y() - m_panel.Y()
                 - (m_panel.MinHeight() - m_tiles[idx].MinHeight()) / 2.0f;
    m_panel.SetScroll(target);
}

// Catalog-relative path of the selected tile
bool MenuState::SelectedImageRelPath(Game& game, std::string& out) const
{
    const auto& catalog = game.GetResources().GetCatalog();
    if (m_selCat < 0 || m_selCat >= (int)catalog.size()) return false;
    if (m_selectedIndex < 0 || m_selectedIndex >= (int)m_tileRefs.size()) return false;

    const auto& ref = m_tileRefs[m_selectedIndex];
    const auto& cat = catalog[m_selCat];
    if (ref.first < 0 || ref.first >= (int)cat.subcategories.size()) return false;

    const auto& sub = cat.subcategories[ref.first];
    if (ref.second < 0 || ref.second >= (int)sub.images.size()) return false;

    out = sub.images[ref.second].relPath;
    return true;
}

void MenuState::HandleRowClick(Game& game, int idx)
{
    if (m_level == 0)
    {
        // Enter a category: show all its images grouped by subcategory
        m_selCat = idx;
        m_level = 1;
        m_scrollOffset = 0;
        m_selectedIndex = -1;
        m_progress.clear();
        if (m_preview.id != 0) { UnloadTexture(m_preview); m_preview = { 0 }; }
        m_previewKey.clear();
        BuildImagePanel(game);
        return;
    }

    // Level 1 — select the image and load its preview
    m_selectedIndex = idx;
    std::string rel;
    if (SelectedImageRelPath(game, rel)) LoadPreview(game, rel);
}

void MenuState::LoadPreview(Game& game, const std::string& relPath)
{
    if (m_previewKey == relPath) return;
    if (m_preview.id != 0) UnloadTexture(m_preview);
    m_preview = { 0 };

    Image img = LoadImage(game.GetResources().ResolvePath(relPath).c_str());
    if (img.data)
    {
        // High-res preview: keep it sharp when displayed at large sizes
        // (the preview box can reach several hundred dp * device scale)
        int ms = 900;
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
        int flat = 0; // tile index inside the category (headers are not tiles)
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
                    m_level = 1;
                    m_progress.clear();
                    m_selectedIndex = -1;
                    BuildImagePanel(game);
                    m_selectedIndex = flat;
                    LoadPreview(game, sub.images[ii].relPath);
                    ScrollToTile(flat);
                    return;
                }
                ++flat;
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
    std::string rel;
    if (!SelectedImageRelPath(game, rel)) return;
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

// Geometry of the buttons above the preview. Both are auto-sized in dp:
// Button adds 12 dp padding per side to the text width and 8 dp vertically.
void MenuState::PreviewActions(Game& game, Rectangle& startBtn,
                               Rectangle& deleteBtn, bool& showDelete) const
{
    const float fsDp = 18.0f;
    const float s = ui::UiScale();
    Font& font = game.GetFont();

    float wStart = MeasureGameText(font, "Начать", (int)(fsDp * s)) / s + 24.0f;
    float wDel   = MeasureGameText(font, "Удалить", (int)(fsDp * s)) / s + 24.0f;
    const float h = fsDp + 16.0f;
    const float gap = 10.0f;

    // Deleting images is a debug-only tool (--debug)
    showDelete = Config::Instance().debugMode && m_level == 1 && m_selectedIndex >= 0;

    float total = showDelete ? wStart + gap + wDel : wStart;
    Rectangle pr = PreviewRect();
    float x = PrevCenterX() - total / 2.0f;
    float y = pr.y + pr.height - h - 16.0f;

    startBtn  = Rectangle{ x, y, wStart, h };
    deleteBtn = Rectangle{ x + wStart + gap, y, wDel, h };
}

// Delete the selected image: file, catalog.txt line and in-memory entry
void MenuState::DeleteSelected(Game& game)
{
    std::string rel;
    if (!SelectedImageRelPath(game, rel)) return;

    game.GetResources().RemoveImage(rel);

    if (m_preview.id != 0) { UnloadTexture(m_preview); m_preview = { 0 }; }
    m_previewKey.clear();
    m_selectedIndex = -1;
    m_hoveredIndex = -1;
    m_progress.erase(rel);

    // Rebuild from the updated catalog, keeping the scroll position
    float keepScroll = m_panel.GetScroll();
    BuildImagePanel(game);
    LayoutPanel();
    m_panel.SetScroll(keepScroll);
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
            std::string rel;
            if (!SelectedImageRelPath(game, rel)) return;
            std::string path = game.GetResources().ResolvePath(rel);
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

    // ---- Image panel refresh (level 1) ----
    if (m_level == 1)
    {
        if (!m_gridReady) BuildImagePanel(game);
        LayoutPanel();
        RefreshTiles(game);
        m_panel.Update();   // wheel + scrollbar dragging
    }

    // Hover: list rows on level 0, tiles on level 1
    m_hoveredIndex = -1;
    int listH = (int)ui::LogH() - LIST_Y - 30;
    bool inListX = (m_level == 1)
        ? (mouse.x >= GridX() - 6 && mouse.x <= GridX() + GridW() + 6)
        : (mouse.x >= ListX() - 6 && mouse.x <= ListX() + ListW() + 6);
    bool inListY = (mouse.y >= LIST_Y - 6 && mouse.y <= LIST_Y - 6 + listH);

    if (m_level == 1)
    {
        if (CheckCollisionPointRec(mouse, m_panel.Rect()))
            for (size_t i = 0; i < m_tiles.size(); ++i)
                if (m_tiles[i].Hovered()) { m_hoveredIndex = (int)i; break; }
    }
    else if (inListX && inListY)
    {
        int relY = (int)mouse.y - LIST_Y + m_scrollOffset;
        int idx = relY / ROW_H;
        if (idx >= 0 && idx < rowCount) m_hoveredIndex = idx;
    }

    // Click on a tile (level 1) or a category row (level 0)
    if (m_level == 1)
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
                HandleRowClick(game, m_panelPressTile);
                return;
            }
        }
    }
    else if (inListX && inListY && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && m_hoveredIndex >= 0)
    {
        HandleRowClick(game, m_hoveredIndex);
        return;
    }

    // Enter key to start (level 1 only)
    if (IsKeyPressed(KEY_ENTER) && m_level == 1 && m_selectedIndex >= 0)
    {
        StartGame(game, tRows, tCols);
        return;
    }

    // Scroll — category rows on level 0; the panel handles its own scrollbar
    // and wheel on level 1
    if (m_level == 0)
    {
        int wheel = GetMouseWheelMove();
        if (wheel != 0)
        {
            m_scrollOffset -= wheel * ROW_H * 3;
            int maxScroll = std::max(0, rowCount * ROW_H - (int)ui::LogH() + LIST_Y + 40);
            m_scrollOffset = std::clamp(m_scrollOffset, 0, maxScroll);
        }
    }

    // Action buttons above the preview: "Начать" and (debug mode) "Удалить"
    Rectangle startRect, deleteRect;
    bool showDelete = false;
    PreviewActions(game, startRect, deleteRect, showDelete);

    ui::Button btnStart(0, 0, 0, 0, "Начать");
    btnStart.UpdateAutoSize(game.GetFont());
    btnStart.SetRect(startRect.x, startRect.y, 0, 0);
    btnStart.SetEnabled(m_level == 1 && m_selectedIndex >= 0);
    if (btnStart.Clicked())
    {
        StartGame(game, tRows, tCols);
        return;
    }

    if (showDelete)
    {
        const float s = ui::UiScale();
        Rectangle dr = { deleteRect.x * s, deleteRect.y * s,
                         deleteRect.width * s, deleteRect.height * s };
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
            CheckCollisionPointRec(mouse, dr))
        {
            DeleteSelected(game);
            return;
        }
    }

    // Back
    float bu = ui::UiScale();
    if (mouse.x >= 15 * bu && mouse.x < 85 * bu && mouse.y >= 15 * bu && mouse.y < 45 * bu &&
        IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        if (m_level > 0)
        {
            m_level = 0;
            m_scrollOffset = 0;
            m_selectedIndex = -1;
            m_selCat = -1;
            m_progress.clear();
            ResetImagePanel();
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

    // Title — categories on level 0, the category name on level 1
    std::string title = "Выберите категорию";
    std::string subTitle = TextFormat("%d категорий  |  %s",
                                      (int)catalog.size(), diffNames[preset]);
    if (m_level == 1 && m_selCat >= 0 && m_selCat < (int)catalog.size())
    {
        const auto& cat = catalog[m_selCat];
        int images = 0;
        for (const auto& sub : cat.subcategories) images += (int)sub.images.size();
        title = cat.name;
        subTitle = TextFormat("%d картинок в %d подкатегориях  |  %s",
                              images, (int)cat.subcategories.size(), diffNames[preset]);
    }

    DrawGameText(font, title.c_str(), ListX(), 62, (int)(26 * ui::UiScale()), TEXT);
    DrawGameText(font, subTitle.c_str(), ListX(), 94, (int) (16 * ui::UiScale()), MUT);

    // Card behind the category list (level 0); the image panel (level 1)
    // draws its own background and scrollbar
    if (m_level == 0)
    {
        int listH = (int)ui::LogH() - LIST_Y - 30;
        DrawRectangle(ListX() - 6, LIST_Y - 6, ListW() + 12, listH, CARD);
        DrawRectangleLines(ListX() - 6, LIST_Y - 6, ListW() + 12, listH, ColorAlpha(MUT, 0.18f));
    }

    if (m_level == 1)
    {
        // ------------- Image panel: subcategory headers + tiles -------------
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
        // ---------------- Category list ----------------
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
    if (m_level == 1)
    {
        Rectangle pr = PreviewRect();          // dp coordinates
        float pv = ui::UiScale();
        // Physical pixels for direct drawing (DrawTexturePro etc.)
        int px = (int)(pr.x * pv), py = (int)(pr.y * pv), pz = (int)(pr.width * pv);

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
            int fs = (int)(18 * ui::UiScale());
            DrawGameText(font, "Превью",
                         px + pz / 2 - (int)MeasureGameText(font, "Превью", fs) / 2,
                         py + pz / 2 - fs / 2, fs, MUT);
        }

        // "Начать" (and in debug mode "Удалить") overlaid at the bottom
        Rectangle startRect, deleteRect;
        bool showDelete = false;
        PreviewActions(game, startRect, deleteRect, showDelete);

        ui::Button b(0, 0, 0, 0, "Начать");
        b.UpdateAutoSize(font);
        b.SetRect(startRect.x, startRect.y, 0, 0);
        b.SetEnabled(m_selectedIndex >= 0);
        b.SetFontSize(18);
        b.SetColors(Color{ 0, 0, 0, 150 }, ColorAlpha(WHITE, 0.6f), WHITE);
        b.SetHoverColors(Color{ 0, 0, 0, 190 }, ACC, WHITE);
        b.SetDisabledColors(Color{ 0, 0, 0, 80 }, ColorAlpha(WHITE, 0.2f), ColorAlpha(WHITE, 0.4f));
        b.Draw(font);

        if (showDelete)
        {
            ui::Button del(0, 0, 0, 0, "Удалить");
            del.UpdateAutoSize(font);
            del.SetRect(deleteRect.x, deleteRect.y, 0, 0);
            del.SetFontSize(18);
            del.SetColors(Color{ 0, 0, 0, 150 }, ColorAlpha(Color{ 235, 90, 90, 255 }, 0.7f),
                          Color{ 255, 200, 200, 255 });
            del.SetHoverColors(Color{ 165, 35, 35, 235 }, Color{ 255, 130, 130, 255 }, WHITE);
            del.Draw(font);
        }
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
