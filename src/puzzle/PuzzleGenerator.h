#ifndef PUZZLEGENERATOR_H
#define PUZZLEGENERATOR_H

#include "puzzle/Piece.h"
#include <string>
#include <vector>

struct PuzzleResult
{
    std::vector<Piece> pieces;
    Texture2D sourceTexture = { 0 };
    int rows = 0;
    int cols = 0;
    float cellSize = 0.0f;
    float imageWidth  = 0.0f;
    float imageHeight = 0.0f;
    float gridOffsetX = 0.0f;  // left edge of the grid in image space
    float gridOffsetY = 0.0f;  // top edge of the grid in image space
};

class PuzzleGenerator
{
public:
    PuzzleGenerator() = default;

    PuzzleResult Generate(const std::string& imagePath,
                          int rows, int cols,
                          int maxTextureSize = 2048);

    static void ScatterPieces(std::vector<Piece>& pieces,
                              float scatterRadius,
                              int seed = 0);

    static void Unload(PuzzleResult& result);
};

#endif // PUZZLEGENERATOR_H
