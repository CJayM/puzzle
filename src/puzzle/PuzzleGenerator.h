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
};

class PuzzleGenerator
{
public:
    PuzzleGenerator() = default;

    PuzzleResult Generate(const std::string& imagePath,
                          int rows, int cols,
                          int maxTextureSize = 2048);

    // Scatter pieces randomly within a radius
    static void ScatterPieces(std::vector<Piece>& pieces,
                              float scatterRadius,
                              int seed = 0);

    // Clean up source texture
    static void Unload(PuzzleResult& result);

private:
    static Piece CreatePiece(int id, int gridX, int gridY,
                             const Image& sourceImage,
                             float cellSize,
                             int texMargin);
};

#endif // PUZZLEGENERATOR_H
