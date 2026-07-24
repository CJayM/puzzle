#ifndef PIECESHAPEGENERATOR_H
#define PIECESHAPEGENERATOR_H

#include "puzzle/Piece.h"
#include "raylib.h"
#include <vector>

class PieceShapeGenerator
{
public:
    PieceShapeGenerator(float cellSize, int margin);

    // Generate profiles for all internal edges of a grid
    void GenerateEdgeProfiles(std::vector<Piece>& pieces,
                              int rows, int cols, unsigned int seed = 12345);

    // Build the polygon outline for a single piece
    std::vector<Vector2> BuildOutline(const Piece& piece, float cellSize) const;

    // Create an alpha-masked texture for a piece from its source image sub-rect
    Texture2D CreateShapedTexture(const Piece& piece,
                                  const Image& sourceImage,
                                  int srcX, int srcY,
                                  int cellSizePx) const;

private:
    // Generate edge points for one side
    static std::vector<Vector2> GenerateEdgePoints(
        Vector2 start, Vector2 end,
        Vector2 outward,
        const EdgeProfile& profile,
        int segments = 10);

    // Fill a polygon on an image using triangle fan
    static void FillPolygon(Image* img, const std::vector<Vector2>& points, Color color);

    // Hash-based deterministic profile generation
    static EdgeProfile GenerateProfile(int edgeId, bool vertical);

    float m_cellSize;
    int m_margin;
};

#endif // PIECESHAPEGENERATOR_H
