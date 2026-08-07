#ifndef SAVEMANAGER_H
#define SAVEMANAGER_H

#include "puzzle/Piece.h"
#include <string>
#include <vector>

#pragma pack(push, 1)
struct SaveHeader
{
    char magic[4] = { 'J', 'P', 'Z', 1 };
    int imageIndex = -1;
    int rows = 0;
    int cols = 0;
    float cellSize = 0.0f;
    float elapsedTime = 0.0f;
    int pieceCount = 0;
};

struct SavePiece
{
    int id = 0;
    int gridX = 0, gridY = 0;
    float posX = 0.0f, posY = 0.0f;
    int rotation = 0;
    int blockId = 0;
};
#pragma pack(pop)

class SaveManager
{
public:
    static std::string SavePath(int imageIndex, int rows, int cols);
    static bool HasSave(int imageIndex, int rows, int cols);

    static bool Save(int imageIndex, int rows, int cols,
                     float cellSize, float elapsedTime,
                     const std::vector<Piece>& pieces);

    static bool Load(int imageIndex, int rows, int cols,
                     float& cellSize, float& elapsedTime,
                     std::vector<Piece>& pieces);

    // Lightweight: load only the header to get elapsed time
    static bool LoadHeader(int imageIndex, int rows, int cols,
                           float& elapsedTime, int& pieceCount);

    static void DeleteSave(int imageIndex, int rows, int cols);
    static void DeleteAllSaves();

    // Track last played puzzle for "Continue"
    static void SetLastPlayed(int imageIndex, int rows, int cols);
    static bool GetLastPlayed(int& imageIndex, int& rows, int& cols);
};

#endif
