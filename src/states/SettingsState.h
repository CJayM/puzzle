#ifndef SETTINGSSTATE_H
#define SETTINGSSTATE_H

#include "states/GameState.h"
#include "ui/Button.h"
#include "ui/Label.h"
#include "raylib.h"

struct SettingsData
{
    bool musicOn = true;
    int difficultyPreset = 2; // 0=25, 1=64, 2=256, 3=1024 pieces
    bool lightBackground = false; // light/dark playfield background
    int uiScale = 1; // 0=small, 1=normal, 2=large (UI buttons/labels)
};

class SettingsState : public GameState
{
public:
    void Enter(Game& game) override;
    void Exit(Game& game) override;
    void Update(Game& game) override;
    void Draw(Game& game) override;

    // Global settings load/save
    static SettingsData& GetSettings();
    static void LoadFromFile();
    static void SaveToFile();

    // UI multiplier from the user's scale preference (0.8 / 1.0 / 1.25)
    static float UserScale();

private:
    void InitUi();                     // create labels/buttons/layouts (Enter)
    void UpdateLayout(Font& font);     // auto sizes + compute positions

    bool m_musicOn = true;
    int m_difficultyPreset = 2;
    int m_uiScale = 1;

    // Widgets (created once, laid out via nested VLayout/HLayout)
    ui::Label m_lblSound;
    ui::Button m_btnMusic;
    ui::Label m_lblDiff;
    ui::Button m_btnDiffs[4];
    ui::Label m_lblScale;
    ui::Button m_btnScales[3];
    ui::Button m_btnBack;

    ui::VLayout m_mainLayout;   // rows + back
    ui::HLayout m_rowSound;     // [Звуки, music]
    ui::HLayout m_rowDiff;      // [Сложность по умолчанию, VLayout(4 diffs)]
    ui::VLayout m_diffCol;      // difficulty buttons column
    ui::HLayout m_rowScale;     // [Размер интерфейса, HLayout(3 scales)]
    ui::HLayout m_scaleRow;     // scale radio buttons row
};

#endif
