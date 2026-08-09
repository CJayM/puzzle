#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include "core/ResourceManager.h"
#include <memory>
#include <string>

class GameState;

class Game
{
public:
    Game();
    ~Game();

    bool Init();
    void Run();
    void Shutdown();

    void ChangeState(std::unique_ptr<GameState> newState);

    ResourceManager& GetResources() { return m_resources; }
    Camera2D& GetCamera() { return m_camera; }
    Font& GetFont() { return m_font; }

    // Selection state — full filesystem path of the chosen image
    std::string selectedImagePath;
    int selectedRows = 4;
    int selectedCols = 4;
    bool continueGame = false;  // true = load saved state instead of fresh
    bool exitRequested = false; // true = close the game

private:
    void Update();
    void Draw();

    bool m_initialized = false;
    Camera2D m_camera = {};
    ResourceManager m_resources;
    Font m_font = { 0 };
    Shader m_fontShader = { 0 }; // SDF shader for the font
    std::unique_ptr<GameState> m_currentState;
};

#endif // GAME_H
