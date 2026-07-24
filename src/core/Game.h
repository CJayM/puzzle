#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include "core/ResourceManager.h"
#include <memory>

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

    // Selection state
    int selectedImageIndex = -1;
    int selectedRows = 4;
    int selectedCols = 4;
    bool continueGame = false;  // true = load saved state instead of fresh

private:
    void Update();
    void Draw();

    bool m_initialized = false;
    Camera2D m_camera = {};
    ResourceManager m_resources;
    std::unique_ptr<GameState> m_currentState;
};

#endif // GAME_H
