#ifndef MENUSTATE_H
#define MENUSTATE_H

#include "states/GameState.h"
#include "ui/Button.h"
#include "ui/Label.h"
#include "raylib.h"
#include <map>
#include <string>
#include <utility>
#include <vector>

class MenuState : public GameState
{
public:
    void Enter(Game& game) override;
    void Exit(Game& game) override;
    void Update(Game& game) override;
    void Draw(Game& game) override;

    // Request that Enter() opens the first unfinished puzzle directly
    static void SetFirstUnfinished() { s_firstUnfinished = true; }

private:
    void HandleRowClick(Game& game, int idx);
    void StartGame(Game& game, int tRows, int tCols);
    void DeleteSelected(Game& game);
    // Rects (dp) of the buttons above the preview: "Начать" and, in debug
    // mode, "Удалить"
    void PreviewActions(Game& game, Rectangle& startBtn, Rectangle& deleteBtn,
                        bool& showDelete) const;
    void LoadPreview(Game& game, const std::string& relPath);
    void SelectFirstUnfinished(Game& game);
    bool IsCompleted(const std::string& path) const;
    int  LevelCount(Game& game) const;
    std::string LevelLabel(Game& game, int idx) const;
    void BuildImagePanel(Game& game);   // rebuild header+tile list for a category
    void ResetImagePanel();
    void LayoutPanel();                 // assign panel rect and lay its content out
    void RefreshTiles(Game& game);      // update textures/fill of visible tiles
    float LoadProgress(Game& game, const std::string& relPath);
    void ScrollToTile(int idx);
    bool SelectedImageRelPath(Game& game, std::string& out) const;

    static bool s_firstUnfinished;

    bool m_confirming = false; // re-assemble confirmation dialog

    int m_level = 0;            // 0 = categories, 1 = all images of the category
    int m_selCat = -1;
    int m_selectedIndex = -1;   // tile index on level 1
    int m_hoveredIndex = -1;
    int m_scrollOffset = 0;
    Texture2D m_preview = { 0 };
    std::string m_previewKey;

    // Completion cache for the tile grid: relPath -> 0..1 (level 1)
    std::map<std::string, float> m_progress;

    // Image panel: section headers + a flow of tile buttons, scrollable.
    // Both vectors are reserved up front — the layout stores their addresses.
    ui::Panel m_panel;
    ui::SectionedFlowLayout m_grid;
    std::vector<ui::Label> m_headers;
    std::vector<ui::Button> m_tiles;
    std::vector<std::pair<int, int>> m_tileRefs; // tile -> (sub, image)
    bool m_gridReady = false;

    // Panel drag-to-scroll / click state (level 2)
    bool m_panelDragging = false;
    float m_panelDragStartY = 0.0f;
    float m_panelDragStartScroll = 0.0f;
    bool m_panelDragMoved = false;
    int m_panelPressTile = -1;
};

#endif // MENUSTATE_H
