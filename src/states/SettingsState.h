#ifndef SETTINGSSTATE_H
#define SETTINGSSTATE_H

#include "states/GameState.h"
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
    void InitLabels();              // create labels once (Enter)

    bool m_musicOn = true;
    int m_difficultyPreset = 2;
    int m_uiScale = 1;

    // Section labels (created once, repositioned every frame in Draw)
    ui::Label m_lblDiff;
    ui::Label m_lblScale;
};

#endif
