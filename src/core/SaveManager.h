#ifndef SAVEMANAGER_H
#define SAVEMANAGER_H

#include "puzzle/Piece.h"
#include <string>
#include <vector>

#pragma pack(push, 1)
struct SaveHeader
{
    char magic[4] = { 'J', 'P', 'Z', 3 };
    char imagePath[192] = { 0 };  // full path of the source image
    int rows = 0;
    int cols = 0;
    float cellSize = 0.0f;
    float elapsedTime = 0.0f;
    int pieceCount = 0;
    int blocksCount = 0;          // number of blocks at save time (progress)
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
    // Saves are keyed by the image's filesystem path (stable across catalog edits)
    static std::string SavePath(const std::string& imagePath, int rows, int cols);
    static bool HasSave(const std::string& imagePath, int rows, int cols);
    static bool HasAnySave();

    static bool Save(const std::string& imagePath, int rows, int cols,
                     float cellSize, float elapsedTime, int blocksCount,
                     const std::vector<Piece>& pieces);

    static bool Load(const std::string& imagePath, int rows, int cols,
                     float& cellSize, float& elapsedTime,
                     std::vector<Piece>& pieces);

    // Lightweight: load only the header to get elapsed time and progress
    static bool LoadHeader(const std::string& imagePath, int rows, int cols,
                           float& elapsedTime, int& pieceCount, int& blocksCount);

    static void DeleteSave(const std::string& imagePath, int rows, int cols);
    static void DeleteAllSaves();

    // Track last played puzzle for "Continue"
    static void SetLastPlayed(const std::string& imagePath, int rows, int cols);
    static bool GetLastPlayed(std::string& imagePath, int& rows, int& cols);
};

#endif
