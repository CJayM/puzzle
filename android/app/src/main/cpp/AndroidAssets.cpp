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

    AAssetManager* mgr = app->activity->assetManager;
    AAssetDir* dir = AAssetManager_openDir(mgr, "images");
    if (dir == nullptr)
    {
        // No "images" folder inside APK assets — nothing to copy
        outImagesPath = dest;
        return true;
    }

    char buffer[8192];
    const char* name = nullptr;
    while ((name = AAssetDir_getNextFileName(dir)) != nullptr)
    {
        std::string assetPath = std::string("images/") + name;
        AAsset* asset = AAssetManager_open(mgr, assetPath.c_str(), AASSET_MODE_STREAMING);
        if (asset == nullptr) continue;

        FILE* out = fopen((dest + "/" + name).c_str(), "wb");
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
    outImagesPath = dest;
    return true;
}

#endif // PLATFORM_ANDROID
