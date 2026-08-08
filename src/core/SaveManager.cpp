#include "core/SaveManager.h"
#include "core/Config.h"
#include "raylib.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#endif

static const char* SaveDirCStr()
{
    return Config::Instance().saveDir.c_str();
}

// FNV-1a hash of the image path — used to build a stable save file name
static unsigned int HashPath(const std::string& s)
{
    unsigned int h = 2166136261u;
    for (char c : s)
    {
        h ^= (unsigned char)c;
        h *= 16777619u;
    }
    return h;
}

std::string SaveManager::SavePath(const std::string& imagePath, int rows, int cols)
{
    return TextFormat("%s/puzzle_%08x_%dx%d.dat", SaveDirCStr(),
                      HashPath(imagePath), rows, cols);
}

bool SaveManager::HasAnySave()
{
    if (!DirectoryExists(SaveDirCStr())) return false;

    // An actual puzzle save must exist (not just the directory or lastplay)
    FilePathList files = LoadDirectoryFiles(SaveDirCStr());
    bool found = false;
    for (unsigned int i = 0; i < files.count && !found; ++i)
    {
        std::string name = files.paths[i];
        if (name.size() > 4 && name.substr(name.size() - 4) == ".dat" &&
            name.find("lastplay") == std::string::npos)
            found = true;
    }
    UnloadDirectoryFiles(files);
    return found;
}

bool SaveManager::HasSave(const std::string& imagePath, int rows, int cols)
{
    std::string path = SavePath(imagePath, rows, cols);
    return FileExists(path.c_str());
}

// Read the save header supporting both v2 (no blocksCount) and v3 formats.
// Returns false on I/O error; 'version' holds the magic[3] value.
static bool ReadSaveHeader(FILE* f, SaveHeader& h, int& version)
{
    if (fread(h.magic, 4, 1, f) != 1) return false;
    if (h.magic[0] != 'J' || h.magic[1] != 'P' || h.magic[2] != 'Z') return false;
    version = h.magic[3];
    if (version != 2 && version != 3) return false;

    size_t rest = sizeof(SaveHeader) - 4;
    if (version == 2) rest -= sizeof(int); // v2 has no blocksCount field
    if (fread((char*)&h + 4, rest, 1, f) != 1) return false;

    if (version == 2) h.blocksCount = h.pieceCount; // unknown -> treat as 0% done
    return true;
}

bool SaveManager::Save(const std::string& imagePath, int rows, int cols,
                        float cellSize, float elapsedTime, int blocksCount,
                        const std::vector<Piece>& pieces)
{
    // Ensure saves directory exists
    if (!DirectoryExists(SaveDirCStr()))
    {
#if defined(_WIN32)
        _mkdir(SaveDirCStr());
#else
        mkdir(SaveDirCStr(), 0755);
#endif
    }

    SaveHeader header;
    strncpy(header.imagePath, imagePath.c_str(), sizeof(header.imagePath) - 1);
    header.rows = rows;
    header.cols = cols;
    header.cellSize = cellSize;
    header.elapsedTime = elapsedTime;
    header.pieceCount = (int)pieces.size();
    header.blocksCount = blocksCount;

    std::string path = SavePath(imagePath, rows, cols);
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;

    fwrite(&header, sizeof(header), 1, f);

    for (const auto& p : pieces)
    {
        SavePiece sp;
        sp.id = p.id;
        sp.gridX = p.gridX;
        sp.gridY = p.gridY;
        sp.posX = p.position.x;
        sp.posY = p.position.y;
        sp.rotation = p.rotation;
        sp.blockId = p.blockId;
        fwrite(&sp, sizeof(sp), 1, f);
    }

    fclose(f);
    return true;
}

bool SaveManager::Load(const std::string& imagePath, int rows, int cols,
                        float& cellSize, float& elapsedTime,
                        std::vector<Piece>& pieces)
{
    std::string path = SavePath(imagePath, rows, cols);
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;

    SaveHeader header;
    int version = 0;
    if (!ReadSaveHeader(f, header, version))
    {
        fclose(f);
        return false;
    }

    // Validate consistency (v2/v3 saves are keyed by the image path)
    if (header.imagePath[0] == 0 || strcmp(header.imagePath, imagePath.c_str()) != 0 ||
        header.rows != rows || header.cols != cols)
    {
        fclose(f);
        return false;
    }

    cellSize = header.cellSize;
    elapsedTime = header.elapsedTime;
    pieces.resize(header.pieceCount);

    for (int i = 0; i < header.pieceCount; ++i)
    {
        SavePiece sp;
        if (fread(&sp, sizeof(sp), 1, f) != 1)
        {
            fclose(f);
            return false;
        }
        Piece& p = pieces[i];
        p.id = sp.id;
        p.gridX = sp.gridX;
        p.gridY = sp.gridY;
        p.position = { sp.posX, sp.posY };
        p.rotation = sp.rotation;
        p.blockId = sp.blockId;
        // Texture is loaded separately in PlayState
        p.texture = { 0 };
        p.flags = 0;
    }

    fclose(f);
    return true;
}

bool SaveManager::LoadHeader(const std::string& imagePath, int rows, int cols,
                              float& elapsedTime, int& pieceCount, int& blocksCount)
{
    std::string path = SavePath(imagePath, rows, cols);
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    SaveHeader h;
    int version = 0;
    if (!ReadSaveHeader(f, h, version) ||
        h.imagePath[0] == 0 || strcmp(h.imagePath, imagePath.c_str()) != 0 ||
        h.rows != rows || h.cols != cols)
    { fclose(f); return false; }
    elapsedTime = h.elapsedTime;
    pieceCount = h.pieceCount;
    blocksCount = h.blocksCount;
    fclose(f);
    return true;
}

void SaveManager::DeleteSave(const std::string& imagePath, int rows, int cols)
{
    std::string path = SavePath(imagePath, rows, cols);
    if (FileExists(path.c_str()))
        std::remove(path.c_str());
}

void SaveManager::DeleteAllSaves()
{
    // Simple approach: delete known pattern files
    // A more robust solution would use directory listing
}

void SaveManager::SetLastPlayed(const std::string& imagePath, int rows, int cols)
{
    if (!DirectoryExists(SaveDirCStr()))
    {
#if defined(_WIN32)
        _mkdir(SaveDirCStr());
#else
        mkdir(SaveDirCStr(), 0755);
#endif
    }

    char buf[192] = { 0 };
    strncpy(buf, imagePath.c_str(), sizeof(buf) - 1);

    FILE* f = fopen(TextFormat("%s/lastplay.dat", SaveDirCStr()), "wb");
    if (!f) return;
    fwrite(buf, sizeof(buf), 1, f);
    fwrite(&rows, sizeof(rows), 1, f);
    fwrite(&cols, sizeof(cols), 1, f);
    fclose(f);
}

bool SaveManager::GetLastPlayed(std::string& imagePath, int& rows, int& cols)
{
    FILE* f = fopen(TextFormat("%s/lastplay.dat", SaveDirCStr()), "rb");
    if (!f) return false;
    char buf[192] = { 0 };
    bool ok = (fread(buf, sizeof(buf), 1, f) == 1 &&
               fread(&rows, sizeof(rows), 1, f) == 1 &&
               fread(&cols, sizeof(cols), 1, f) == 1);
    fclose(f);
    imagePath = buf;
    return ok && !imagePath.empty();
}
