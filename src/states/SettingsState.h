#ifndef SETTINGSSTATE_H
#define SETTINGSSTATE_H

#include "states/GameState.h"
#include "raylib.h"

struct SettingsData
{
    bool musicOn = true;
    int difficultyPreset = 2; // 0=25, 1=64, 2=256, 3=1024 pieces
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

private:
    bool m_musicOn = true;
    int m_difficultyPreset = 2;
};

#endif
