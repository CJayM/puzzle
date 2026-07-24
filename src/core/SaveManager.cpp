#include "core/SaveManager.h"
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

std::string SaveManager::SavePath(int imageIndex, int rows, int cols)
{
    return TextFormat("saves/puzzle_%d_%dx%d.dat", imageIndex, rows, cols);
}

bool SaveManager::HasSave(int imageIndex, int rows, int cols)
{
    std::string path = SavePath(imageIndex, rows, cols);
    return FileExists(path.c_str());
}

bool SaveManager::Save(int imageIndex, int rows, int cols,
                        float cellSize, float elapsedTime,
                        const std::vector<Piece>& pieces)
{
    // Ensure saves directory exists
    if (!DirectoryExists("saves"))
    {
#if defined(_WIN32)
        _mkdir("saves");
#else
        mkdir("saves", 0755);
#endif
    }

    SaveHeader header;
    header.imageIndex = imageIndex;
    header.rows = rows;
    header.cols = cols;
    header.cellSize = cellSize;
    header.elapsedTime = elapsedTime;
    header.pieceCount = (int)pieces.size();

    std::string path = SavePath(imageIndex, rows, cols);
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

bool SaveManager::Load(int imageIndex, int rows, int cols,
                        float& cellSize, float& elapsedTime,
                        std::vector<Piece>& pieces)
{
    std::string path = SavePath(imageIndex, rows, cols);
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;

    SaveHeader header;
    if (fread(&header, sizeof(header), 1, f) != 1)
    {
        fclose(f);
        return false;
    }

    // Validate magic and consistency
    if (header.magic[0] != 'J' || header.magic[1] != 'P' ||
        header.magic[2] != 'Z' || header.imageIndex != imageIndex ||
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

void SaveManager::DeleteSave(int imageIndex, int rows, int cols)
{
    std::string path = SavePath(imageIndex, rows, cols);
    if (FileExists(path.c_str()))
        std::remove(path.c_str());
}

void SaveManager::DeleteAllSaves()
{
    // Simple approach: delete known pattern files
    // A more robust solution would use directory listing
}
