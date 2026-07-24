#include "core/Game.h"
#include "core/Config.h"
#include "states/GameState.h"
#include "states/MenuState.h"
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

    // Start with MenuState
    ChangeState(std::make_unique<MenuState>());

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

void Game::ChangeState(std::unique_ptr<GameState> newState)
{
    if (m_currentState)
    {
        m_currentState->Exit(*this);
    }
    m_currentState = std::move(newState);
    if (m_currentState)
    {
        m_currentState->Enter(*this);
    }
}

void Game::Update()
{
    const auto& cfg = Config::Instance();
    m_camera.offset = {
        static_cast<float>(GetScreenWidth())  / 2.0f,
        static_cast<float>(GetScreenHeight()) / 2.0f
    };

    if (m_currentState)
    {
        m_currentState->Update(*this);
    }
}

void Game::Draw()
{
    BeginDrawing();

    if (m_currentState)
    {
        BeginMode2D(m_camera);
        m_currentState->Draw(*this);
        EndMode2D();
    }
    else
    {
        ClearBackground(RAYWHITE);
    }

    DrawFPS(10, 10);
    EndDrawing();
}

void Game::Shutdown()
{
    if (m_initialized)
    {
        m_currentState.reset();
        CloseWindow();
        m_initialized = false;
    }
}
