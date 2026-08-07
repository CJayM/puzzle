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
    std::string saveDir = "saves"; // override on Android to internalDataPath

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
