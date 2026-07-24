#ifndef GAME_H
#define GAME_H

#include "raylib.h"

class Game
{
public:
    Game();
    ~Game();

    bool Init();
    void Run();
    void Shutdown();

private:
    void Update();
    void Draw();

    bool m_initialized = false;
    Camera2D m_camera = {};
};

#endif // GAME_H
