#include "core/ResourceManager.h"
#include "raylib.h"

#include <algorithm>
#include <cstring>

ResourceManager::~ResourceManager()
{
    UnloadAll();
}

bool ResourceManager::IsImageExtension(const std::string& ext)
{
    const char* valid[] = { ".png", ".jpg", ".jpeg", ".bmp" };
    for (auto* v : valid)
    {
        if (ext == v) return true;
    }
    return false;
}

void ResourceManager::ScanDirectory(const std::string& path)
{
    // Unload any previously loaded data
    UnloadAll();
    m_images.clear();

    FilePathList files = LoadDirectoryFiles(path.c_str());
    if (files.count == 0)
    {
        UnloadDirectoryFiles(files);
        return;
    }

    for (unsigned int i = 0; i < files.count; ++i)
    {
        std::string fullPath = files.paths[i];

        // Find extension
        auto dot = fullPath.rfind('.');
        if (dot == std::string::npos) continue;

        std::string ext;
        for (auto c : fullPath.substr(dot))
            ext += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        if (!IsImageExtension(ext)) continue;

        // Extract just the filename
        auto sep = fullPath.rfind('\\');
        if (sep == std::string::npos) sep = fullPath.rfind('/');
        std::string name = (sep == std::string::npos) ? fullPath : fullPath.substr(sep + 1);

        m_images.push_back({ fullPath, name });
    }

    UnloadDirectoryFiles(files);

    // Sort by filename for deterministic order
    std::sort(m_images.begin(), m_images.end(),
        [](const ImageInfo& a, const ImageInfo& b) { return a.fileName < b.fileName; });
}

Texture2D ResourceManager::LoadThumbnail(int index, int maxSize)
{
    if (index < 0 || index >= static_cast<int>(m_images.size()))
        return { 0 };

    // Ensure textures vector is large enough
    if (index >= static_cast<int>(m_textures.size()))
        m_textures.resize(m_images.size(), Texture2D{ 0 });

    // Return cached if already loaded
    if (m_textures[index].id != 0)
        return m_textures[index];

    Image img = LoadImage(m_images[index].filePath.c_str());
    if (img.data == nullptr)
        return { 0 };

    // Resize if larger than maxSize
    if (img.width > maxSize || img.height > maxSize)
    {
        float scale = static_cast<float>(maxSize) / std::max(img.width, img.height);
        ImageResize(&img,
            static_cast<int>(img.width * scale),
            static_cast<int>(img.height * scale));
    }

    m_textures[index] = LoadTextureFromImage(img);
    UnloadImage(img);

    return m_textures[index];
}

Texture2D ResourceManager::LoadFullImage(int index, int maxSize)
{
    if (index < 0 || index >= static_cast<int>(m_images.size()))
        return { 0 };

    // For full image we don't cache by default (could be large), but we return a fresh texture
    // The caller must manage this texture's lifecycle
    Image img = LoadImage(m_images[index].filePath.c_str());
    if (img.data == nullptr)
        return { 0 };

    // Resize if larger than maxSize
    if (img.width > maxSize || img.height > maxSize)
    {
        float scale = static_cast<float>(maxSize) / std::max(img.width, img.height);
        ImageResize(&img,
            static_cast<int>(img.width * scale),
            static_cast<int>(img.height * scale));
    }

    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);

    return tex;
}

void ResourceManager::UnloadTexture(int index)
{
    if (index >= 0 && index < static_cast<int>(m_textures.size()) && m_textures[index].id != 0)
    {
        ::UnloadTexture(m_textures[index]);
        m_textures[index] = Texture2D{ 0 };
    }
}

void ResourceManager::UnloadAll()
{
    for (auto& tex : m_textures)
    {
        if (tex.id != 0)
            ::UnloadTexture(tex);
    }
    m_textures.clear();
}
