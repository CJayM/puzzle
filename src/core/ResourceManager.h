#ifndef RESOURCEMANAGER_H
#define RESOURCEMANAGER_H

#include "raylib.h"
#include <string>
#include <vector>

struct CatalogImage
{
    std::string relPath;   // path relative to the images directory, e.g. "planets/earth.png"
    std::string fileName;  // just the base name for display
};

struct CatalogSubcategory
{
    std::string name;
    std::vector<CatalogImage> images;
};

struct CatalogCategory
{
    std::string name;
    std::vector<CatalogSubcategory> subcategories;
};

class ResourceManager
{
public:
    ResourceManager() = default;
    ~ResourceManager();

    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    // Load the image catalog from a text file (categories -> subcategories -> image paths).
    // Falls back to scanning the images directory when the file is missing.
    void LoadCatalog(const std::string& catalogPath, const std::string& imagesDir);

    // Access the catalog
    const std::vector<CatalogCategory>& GetCatalog() const { return m_catalog; }
    bool HasImages() const;

    // Resolve a catalog-relative path to a full filesystem path
    std::string ResolvePath(const std::string& relPath) const;

    // Generate (or reuse from cache) a square thumbnail file for a catalog image.
    // Returns the cached file path ("" on failure).
    std::string EnsureThumbnail(const std::string& relPath, int size);

    // Load textures by path (cached thumbnails / fresh full image)
    Texture2D LoadThumbnailPath(const std::string& relPath, int maxSize = 256);
    Texture2D LoadFullImagePath(const std::string& relPath, int maxSize = 2048);
    void UnloadAll();

private:
    static bool IsImageExtension(const std::string& ext);
    void ScanDirectoryFallback(const std::string& path);
    void ParseCatalogLine(int indent, const std::string& line);

    std::vector<CatalogCategory> m_catalog;
    std::string m_imagesDir;

    // Simple thumbnail cache keyed by catalog-relative path
    struct ThumbEntry { std::string key; Texture2D tex; };
    std::vector<ThumbEntry> m_thumbs;
};

#endif // RESOURCEMANAGER_H
