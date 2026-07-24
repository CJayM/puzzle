#include "core/Game.h"
#include "core/Config.h"
#include "core/SaveManager.h"
#include "states/MainMenuState.h"
#include "states/SettingsState.h"
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

    // Initialise audio
    InitAudioDevice();

    // Load Arial font
    m_font = LoadFontEx("C:/Windows/Fonts/arial.ttf", 20, 0, 0);
    if (m_font.texture.id == 0)
    {
        TraceLog(LOG_WARNING, "Failed to load Arial font, using default");
        m_font = GetFontDefault();
    }
    SetTextureFilter(m_font.texture, TEXTURE_FILTER_BILINEAR);

    // Initialise camera
    m_camera.target = { 0.0f, 0.0f };
    m_camera.offset = {
        static_cast<float>(cfg.screenWidth)  / 2.0f,
        static_cast<float>(cfg.screenHeight) / 2.0f
    };
    m_camera.rotation = 0.0f;
    m_camera.zoom = cfg.defaultZoom;

    // Load settings
    SettingsState::LoadFromFile();

    // Scan for images before entering menu
    m_resources.ScanDirectory(cfg.imagesPath);

    // Start with MainMenuState
    ChangeState(std::make_unique<MainMenuState>());

    m_initialized = true;
    return true;
}

void Game::Run()
{
    if (!m_initialized) return;

    while (!WindowShouldClose() && !exitRequested)
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
        m_currentState->Draw(*this);
    }
    else
    {
        ClearBackground(RAYWHITE);
    }

    EndDrawing();
}

void Game::Shutdown()
{
    if (m_initialized)
    {
        m_currentState.reset();
        if (m_font.texture.id != 0) UnloadFont(m_font);
        CloseAudioDevice();
        CloseWindow();
        m_initialized = false;
    }
}
