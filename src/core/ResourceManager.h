#ifndef RESOURCEMANAGER_H
#define RESOURCEMANAGER_H

#include "raylib.h"
#include <string>
#include <vector>

struct ImageInfo
{
    std::string filePath;   // full path
    std::string fileName;   // just the filename
};

class ResourceManager
{
public:
    ResourceManager() = default;
    ~ResourceManager();

    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    // Scan a directory and collect image files
    void ScanDirectory(const std::string& path);

    // Access scanned images
    const std::vector<ImageInfo>& GetImageList() const { return m_images; }
    bool HasImages() const { return !m_images.empty(); }

    // Load / unload textures
    Texture2D LoadThumbnail(int index, int maxSize = 256);
    Texture2D LoadFullImage(int index, int maxSize = 2048);
    void UnloadTexture(int index);
    void UnloadAll();

private:
    static bool IsImageExtension(const std::string& ext);

    std::vector<ImageInfo> m_images;
    std::vector<Texture2D> m_textures;  // parallel vector, may contain {0} for unloaded
};

#endif // RESOURCEMANAGER_H
