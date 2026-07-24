#ifndef GAME_H
#define GAME_H

#include "raylib.h"
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

private:
    void Update();
    void Draw();

    bool m_initialized = false;
    Camera2D m_camera = {};
    std::unique_ptr<GameState> m_currentState;
};

#endif // GAME_H
