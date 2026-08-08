#include "core/Game.h"
#include "core/Config.h"
#include "core/SaveManager.h"
#include "states/MainMenuState.h"
#include "states/SettingsState.h"
#include "raylib.h"

#include <vector>

#if defined(PLATFORM_ANDROID)
#include "AndroidAssets.h"
#endif

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

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(cfg.screenWidth, cfg.screenHeight, cfg.windowTitle);
    SetTargetFPS(cfg.targetFPS);

    // Initialise audio
    InitAudioDevice();

    // Load Arial font with Latin + Cyrillic + UI glyph codepoints.
    // On Android the font is bundled in APK assets (fonts/arial.ttf).
#if defined(PLATFORM_ANDROID)
    const char* fontPath = "fonts/arial.ttf";
#else
    const char* fontPath = "C:/Windows/Fonts/arial.ttf";
#endif

    std::vector<int> cps;
    for (int cp = 32; cp <= 126; ++cp) cps.push_back(cp);          // ASCII
    for (int cp = 0x400; cp <= 0x4FF; ++cp) cps.push_back(cp);     // Cyrillic
    const int extra[] = { 0x2013, 0x2014, 0x2190, 0x2192, 0x25B8, 0x25CB, 0x25CF };
    for (int cp : extra) cps.push_back(cp);

    m_font = LoadFontEx(fontPath, 20, cps.data(), (int)cps.size());
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

#if defined(PLATFORM_ANDROID)
    // APK assets cannot be listed via opendir; extract images to internal
    // storage on first run, then scan the extracted directory instead.
    std::string extractedImages;
    if (AndroidEnsureImagesExtracted(extractedImages))
    {
        Config::Instance().imagesPath = extractedImages;
    }

    // Saves must live in internal storage (mkdir("saves") fails on "/")
    std::string saveDir;
    if (AndroidEnsureSaveDir(saveDir))
    {
        Config::Instance().saveDir = saveDir;
    }

    // Thumbnail cache also lives in internal storage
    std::string cacheDir;
    if (AndroidEnsureCacheDir(cacheDir))
    {
        Config::Instance().cacheDir = cacheDir;
    }
#endif

    // Load the image catalog (categories -> subcategories -> images);
    // falls back to scanning the images directory if the file is missing
    m_resources.LoadCatalog(cfg.imagesPath + "/" + cfg.catalogFile, cfg.imagesPath);
    // Thumbnails are generated lazily (on first view of each subcategory) to
    // keep startup instant; results are cached to disk for later runs.

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
