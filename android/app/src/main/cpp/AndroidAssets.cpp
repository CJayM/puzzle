#if defined(PLATFORM_ANDROID)

#include "AndroidAssets.h"

#include <android/asset_manager.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#include <sys/stat.h>
#include <cstdio>
#include <string>

// Declared in raylib's rcore_android.c, NOT exported in raylib.h.
extern "C" struct android_app* GetAndroidApp(void);

#define TAG "Jigsaw"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

static void MakeDirs(const std::string& path)
{
    // Create each missing component of the path
    size_t pos = 0;
    while ((pos = path.find('/', pos + 1)) != std::string::npos)
    {
        std::string part = path.substr(0, pos);
        if (part.empty()) continue;
        mkdir(part.c_str(), 0755);
    }
    mkdir(path.c_str(), 0755);
}

// Recursively copy an assets folder (e.g. "images") into a destination
// directory, preserving the subfolder structure.
static void CopyAssetDir(AAssetManager* mgr,
                         const std::string& assetDir,
                         const std::string& destDir)
{
    MakeDirs(destDir);

    AAssetDir* dir = AAssetManager_openDir(mgr, assetDir.c_str());
    if (dir == nullptr) return;

    char buffer[8192];
    const char* name = nullptr;
    while ((name = AAssetDir_getNextFileName(dir)) != nullptr)
    {
        std::string entry = name;
        if (!entry.empty() && entry.back() == '/')
        {
            // Subdirectory — recurse (AAssetDir appends '/' to folder names)
            entry.pop_back();
            CopyAssetDir(mgr, assetDir + "/" + entry, destDir + "/" + entry);
            continue;
        }

        std::string assetPath = assetDir + "/" + entry;
        AAsset* asset = AAssetManager_open(mgr, assetPath.c_str(), AASSET_MODE_STREAMING);
        if (asset == nullptr) continue;

        FILE* out = fopen((destDir + "/" + entry).c_str(), "wb");
        if (out != nullptr)
        {
            int n = 0;
            while ((n = AAsset_read(asset, buffer, sizeof(buffer))) > 0)
            {
                fwrite(buffer, 1, static_cast<size_t>(n), out);
            }
            fclose(out);
        }

        AAsset_close(asset);
    }

    AAssetDir_close(dir);
}

bool AndroidEnsureImagesExtracted(std::string& outImagesPath)
{
    struct android_app* app = GetAndroidApp();
    if (app == nullptr || app->activity == nullptr || app->activity->assetManager == nullptr)
    {
        LOGE("Cannot access android_app / assetManager");
        return false;
    }

    std::string dest = app->activity->internalDataPath;
    if (dest.empty()) dest = "/data/data/io.github.puzzle.jigsaw/files";
    dest += "/images";

    MakeDirs(dest);

    CopyAssetDir(app->activity->assetManager, "images", dest);

    outImagesPath = dest;
    return true;
}

bool AndroidEnsureSaveDir(std::string& outSaveDir)
{
    struct android_app* app = GetAndroidApp();
    if (app == nullptr || app->activity == nullptr)
    {
        LOGE("Cannot access android_app for save dir");
        return false;
    }

    std::string dir = app->activity->internalDataPath;
    if (dir.empty()) dir = "/data/data/io.github.puzzle.jigsaw/files";
    dir += "/saves";

    MakeDirs(dir);
    outSaveDir = dir;
    return true;
}

bool AndroidEnsureCacheDir(std::string& outCacheDir)
{
    struct android_app* app = GetAndroidApp();
    if (app == nullptr || app->activity == nullptr)
    {
        LOGE("Cannot access android_app for cache dir");
        return false;
    }

    std::string dir = app->activity->internalDataPath;
    if (dir.empty()) dir = "/data/data/io.github.puzzle.jigsaw/files";
    dir += "/cache";

    MakeDirs(dir);
    outCacheDir = dir;
    return true;
}

#endif // PLATFORM_ANDROID
