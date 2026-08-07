#pragma once

#if defined(PLATFORM_ANDROID)

#include <string>

// Copies APK assets/images/* into the app's internal storage on first run.
// Returns the absolute path of the extracted images directory.
bool AndroidEnsureImagesExtracted(std::string& outImagesPath);

#endif // PLATFORM_ANDROID
