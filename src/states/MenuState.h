#ifndef MENUSTATE_H
#define MENUSTATE_H

#include "states/GameState.h"
#include "ui/Button.h"
#include "raylib.h"
#include <map>
#include <string>
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
    void HandleRowClick(Game& game, int idx, int tRows, int tCols);
    void StartGame(Game& game, int tRows, int tCols);
    void LoadPreview(Game& game, const std::string& relPath);
    void SelectFirstUnfinished(Game& game);
    bool IsCompleted(const std::string& path) const;
    int  LevelCount(Game& game) const;
    std::string LevelLabel(Game& game, int idx) const;
    void BuildGrid(Game& game);         // rebuild thumbnail tile buttons
    void RefreshGrid(Game& game);       // update textures/fill each frame
    void EnsureProgressCache(Game& game);

    static bool s_firstUnfinished;

    bool m_confirming = false; // re-assemble confirmation dialog

    int m_level = 0;            // 0 = categories, 1 = subcategories, 2 = images
    int m_selCat = -1;
    int m_selSub = -1;
    int m_selectedIndex = -1;   // image index on level 2
    int m_hoveredIndex = -1;
    int m_scrollOffset = 0;
    Texture2D m_preview = { 0 };
    std::string m_previewKey;

    // Completion cache for the tile grid: relPath -> 0..1 (level 2)
    std::map<std::string, float> m_progress;

    // Thumbnail grid: scrollable panel containing a flow layout of tile buttons
    ui::Panel m_panel;
    ui::FlowLayout m_grid;
    std::vector<ui::Button> m_tiles;
    bool m_gridReady = false;

    // Panel drag-to-scroll / click state (level 2)
    bool m_panelDragging = false;
    float m_panelDragStartY = 0.0f;
    float m_panelDragStartScroll = 0.0f;
    bool m_panelDragMoved = false;
    int m_panelPressTile = -1;
};

#endif // MENUSTATE_H
