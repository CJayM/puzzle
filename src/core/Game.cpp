#include "core/Game.h"
#include "core/Config.h"
#include "raylib.h"

Game::Game()
{
}

Game::~Game()
{
    Shutdown();
}

bool Game::Init()
{
    const auto& cfg = Config::Instance();

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(cfg.screenWidth, cfg.screenHeight, cfg.windowTitle);
    SetTargetFPS(cfg.targetFPS);

    // Initialise camera
    m_camera.target = { 0.0f, 0.0f };
    m_camera.offset = {
        static_cast<float>(cfg.screenWidth)  / 2.0f,
        static_cast<float>(cfg.screenHeight) / 2.0f
    };
    m_camera.rotation = 0.0f;
    m_camera.zoom = cfg.defaultZoom;

    m_initialized = true;
    return true;
}

void Game::Run()
{
    if (!m_initialized) return;

    while (!WindowShouldClose())
    {
        Update();
        Draw();
    }
}

void Game::Update()
{
    // Update camera offset when window is resized
    const auto& cfg = Config::Instance();
    m_camera.offset = {
        static_cast<float>(GetScreenWidth())  / 2.0f,
        static_cast<float>(GetScreenHeight()) / 2.0f
    };
}

void Game::Draw()
{
    BeginDrawing();
    ClearBackground(RAYWHITE);

    BeginMode2D(m_camera);
    // Game content will be drawn here in future steps
    DrawRectangle(-200, -200, 400, 400, LIGHTGRAY);
    DrawText("Jigsaw Puzzle", -180, -20, 20, DARKGRAY);
    EndMode2D();

    DrawFPS(10, 10);

    EndDrawing();
}

void Game::Shutdown()
{
    if (m_initialized)
    {
        CloseWindow();
        m_initialized = false;
    }
}
