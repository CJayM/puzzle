#ifndef CONFIG_H
#define CONFIG_H

#include <string>

struct Config
{
    static Config& Instance();

    // Window
    int screenWidth  = 1280;
    int screenHeight = 720;
    const char* windowTitle = "Jigsaw Puzzle";

    // Paths
    std::string assetsPath = "assets";
    std::string imagesPath = "assets/images";
    std::string catalogFile = "catalog.txt"; // image catalog inside imagesPath
    std::string saveDir = "saves"; // override on Android to internalDataPath
    std::string cacheDir = "cache"; // thumbnails; Android → internalDataPath/cache

    // Device pixel density: physical px per "virtual px" (Android dp / 160).
    // Desktop = 1.0. Set from the device in Game::Init.
    float dpiScale = 1.0f;

    // Debug mode: enabled by the --debug command line key. Adds the
    // destructive tools to the UI (e.g. deleting an image from the library).
    bool debugMode = false;

    // Rendering
    int targetFPS = 60;

    // Camera
    float defaultZoom = 1.0f;
    float minZoom = 0.2f;
    float maxZoom = 4.0f;
    float zoomFactor = 1.15f;

    // Puzzle
    float cellMarginFactor = 0.25f;  // margin for tabs as fraction of cell size

    // Snap
    float snapDistanceFactor = 0.15f; // snap threshold as fraction of cell size

private:
    Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
};

#endif // CONFIG_H
