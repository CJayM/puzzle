#include "core/ResourceManager.h"
#include "core/Config.h"
#include "raylib.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>

#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#endif

ResourceManager::~ResourceManager()
{
    UnloadAll();
}

bool ResourceManager::IsImageExtension(const std::string& ext)
{
    const char* valid[] = { ".png", ".jpg", ".jpeg", ".bmp", ".webp" };
    for (auto* v : valid)
    {
        if (ext == v) return true;
    }
    return false;
}

// ------------------------------------------------------------
// Catalog loading
// ------------------------------------------------------------
void ResourceManager::LoadCatalog(const std::string& catalogPath, const std::string& imagesDir)
{
    UnloadAll();
    m_catalog.clear();
    m_imagesDir = imagesDir;

    std::ifstream in(catalogPath);
    if (!in.is_open())
    {
        // No catalog file — fall back to scanning the directory
        ScanDirectoryFallback(imagesDir);
        return;
    }

    std::string line;
    while (std::getline(in, line))
    {
        // Trim trailing whitespace
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r'))
            line.pop_back();

        if (line.empty()) continue;

        // Count leading indentation
        int indent = 0;
        while (indent < (int)line.size() && (line[indent] == ' ' || line[indent] == '\t'))
            ++indent;

        std::string content = line.substr(indent);

        // Comment
        if (content.empty() || content[0] == '#' || content[0] == ';') continue;

        // Section header [Name]
        if (content[0] == '[')
        {
            size_t close = content.find(']');
            std::string name = (close == std::string::npos)
                ? content.substr(1)
                : content.substr(1, close - 1);
            // Trim name
            size_t b = name.find_first_not_of(" \t");
            size_t e = name.find_last_not_of(" \t");
            name = (b == std::string::npos) ? "" : name.substr(b, e - b + 1);
            if (name.empty()) continue;

            if (indent == 0)
            {
                // New category
                m_catalog.push_back({ name, {} });
            }
            else if (!m_catalog.empty())
            {
                // New subcategory under the current category
                m_catalog.back().subcategories.push_back({ name, {} });
            }
            continue;
        }

        // Image path line (only meaningful inside a subcategory)
        if (!m_catalog.empty() && !m_catalog.back().subcategories.empty())
        {
            // Skip non-image extensions
            auto dot = content.rfind('.');
            if (dot == std::string::npos) continue;
            std::string ext = content.substr(dot);
            std::transform(ext.begin(), ext.end(), ext.begin(),
                [](unsigned char c) { return (char)std::tolower(c); });
            if (!IsImageExtension(ext)) continue;

            std::string rel = content;
            std::string name = rel;
            auto sep = name.rfind('\\');
            if (sep == std::string::npos) sep = name.rfind('/');
            if (sep != std::string::npos) name = name.substr(sep + 1);

            m_catalog.back().subcategories.back().images.push_back({ rel, name });
        }
    }

    // Drop empty categories / subcategories
    for (auto& cat : m_catalog)
    {
        cat.subcategories.erase(
            std::remove_if(cat.subcategories.begin(), cat.subcategories.end(),
                [](const CatalogSubcategory& s) { return s.images.empty(); }),
            cat.subcategories.end());
    }
    m_catalog.erase(
        std::remove_if(m_catalog.begin(), m_catalog.end(),
            [](const CatalogCategory& c) { return c.subcategories.empty(); }),
        m_catalog.end());
}

bool ResourceManager::HasImages() const
{
    for (const auto& cat : m_catalog)
        for (const auto& sub : cat.subcategories)
            if (!sub.images.empty()) return true;
    return false;
}

std::string ResourceManager::ResolvePath(const std::string& relPath) const
{
    if (relPath.empty()) return {};
    // Absolute paths are used as-is
    if (relPath[0] == '/' || (relPath.size() > 2 && relPath[1] == ':'))
        return relPath;
    if (m_imagesDir.empty()) return relPath;
    return m_imagesDir + "/" + relPath;
}

// ------------------------------------------------------------
// Fallback: plain directory scan (no catalog file)
// ------------------------------------------------------------
void ResourceManager::ScanDirectoryFallback(const std::string& path)
{
    FilePathList files = LoadDirectoryFiles(path.c_str());
    if (files.count == 0)
    {
        UnloadDirectoryFiles(files);
        return;
    }

    m_catalog.push_back({ "Images", {} });
    m_catalog.back().subcategories.push_back({ "All", {} });

    for (unsigned int i = 0; i < files.count; ++i)
    {
        std::string fullPath = files.paths[i];

        auto dot = fullPath.rfind('.');
        if (dot == std::string::npos) continue;

        std::string ext = fullPath.substr(dot);
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char c) { return (char)std::tolower(c); });

        if (!IsImageExtension(ext)) continue;

        auto sep = fullPath.rfind('\\');
        if (sep == std::string::npos) sep = fullPath.rfind('/');
        std::string name = (sep == std::string::npos) ? fullPath : fullPath.substr(sep + 1);

        // Keep only the file name as the relative path (flat scan)
        m_catalog.back().subcategories.back().images.push_back({ name, name });
    }

    UnloadDirectoryFiles(files);

    std::sort(m_catalog.back().subcategories.back().images.begin(),
              m_catalog.back().subcategories.back().images.end(),
              [](const CatalogImage& a, const CatalogImage& b)
              { return a.fileName < b.fileName; });
}

// ------------------------------------------------------------
// Thumbnail cache
// ------------------------------------------------------------
std::string ResourceManager::EnsureThumbnail(const std::string& relPath, int size)
{
    if (relPath.empty() || size <= 0) return {};

    std::string src = ResolvePath(relPath);

    // Make sure the cache directory exists
    const std::string& cache = Config::Instance().cacheDir;
    if (!DirectoryExists(cache.c_str()))
    {
#if defined(_WIN32)
        _mkdir(cache.c_str());
#else
        mkdir(cache.c_str(), 0755);
#endif
    }

    // Stable cache file name from the relative path + size
    unsigned int h = 2166136261u;
    for (char c : relPath)
    {
        h ^= (unsigned char)c;
        h *= 16777619u;
    }
    std::string thumbPath = TextFormat("%s/thumb_%08x_%d.png", cache.c_str(), h, size);

    // Reuse the cached file if it's not older than the source image.
    // NOTE: FileExists/GetFileModTime only work on the real filesystem —
    // on Android the source lives inside the APK assets, so we can't
    // stat it; only the cache file (real path) is checked here.
    if (FileExists(thumbPath.c_str()))
    {
        long tThumb = GetFileModTime(thumbPath.c_str());
        long tSrc = GetFileModTime(src.c_str());
        if (tThumb >= tSrc) return thumbPath;
    }

    // Load the source (works for APK assets via raylib's wrapped fopen)
    Image img = LoadImage(src.c_str());
    if (img.data == nullptr) return {};

    // Fit the image into a square canvas with a white background
    Image thumb = GenImageColor(size, size, Color{ 255, 255, 255, 255 });
    float sc = (float)size / (float)std::max(img.width, img.height);
    int w = std::max(1, (int)(img.width * sc));
    int hh = std::max(1, (int)(img.height * sc));
    if (img.width > size || img.height > size)
        ImageResize(&img, w, hh);
    ImageDraw(&thumb, img,
              Rectangle{ 0, 0, (float)img.width, (float)img.height },
              Rectangle{ (size - w) / 2.0f, (size - hh) / 2.0f, (float)w, (float)hh },
              WHITE);

    bool ok = ExportImage(thumb, thumbPath.c_str());
    UnloadImage(img);
    UnloadImage(thumb);

    if (!ok) return {};
    return thumbPath;
}

// ------------------------------------------------------------
// Texture helpers
// ------------------------------------------------------------
Texture2D ResourceManager::LoadThumbnailPath(const std::string& relPath, int maxSize)
{
    // Cached in memory?
    for (auto& e : m_thumbs)
        if (e.key == relPath) return e.tex;

    // Cached on disk (or generate now)
    std::string thumbPath = EnsureThumbnail(relPath, maxSize);
    if (thumbPath.empty()) return { 0 };

    Image img = LoadImage(thumbPath.c_str());
    if (img.data == nullptr) return { 0 };

    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);

    m_thumbs.push_back({ relPath, tex });
    return tex;
}

Texture2D ResourceManager::LoadFullImagePath(const std::string& relPath, int maxSize)
{
    Image img = LoadImage(ResolvePath(relPath).c_str());
    if (img.data == nullptr) return { 0 };

    if (img.width > maxSize || img.height > maxSize)
    {
        float scale = (float)maxSize / std::max(img.width, img.height);
        ImageResize(&img, (int)(img.width * scale), (int)(img.height * scale));
    }

    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    return tex;
}

void ResourceManager::UnloadAll()
{
    for (auto& e : m_thumbs)
        if (e.tex.id != 0) ::UnloadTexture(e.tex);
    m_thumbs.clear();
}
