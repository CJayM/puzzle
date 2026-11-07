#ifndef PIECESHAPEGENERATOR_H
#define PIECESHAPEGENERATOR_H

#include "puzzle/Piece.h"
#include "raylib.h"
#include <vector>

class PieceShapeGenerator
{
public:
    // Клетка прямоугольная: ширина и высота картинки делятся на cols/rows,
    // поэтому пазл сохраняет пропорции изображения
    PieceShapeGenerator(float cellW, float cellH, int margin);

    // Generate profiles for all internal edges of a grid
    void GenerateEdgeProfiles(std::vector<Piece>& pieces,
                              int rows, int cols, unsigned int seed = 12345);

    // Build the polygon outline for a single piece (cell-local coords,
    // (0,0) = top-left of the cell)
    std::vector<Vector2> BuildOutline(const Piece& piece, float cellW, float cellH) const;

    // Alpha mask of the piece shape (cell + margin on each side, GRAY_ALPHA):
    // filled inside the outline, transparent outside
    Image BuildPieceMask(const Piece& piece, int cellWPx, int cellHPx) const;

    // Create an alpha-masked texture for a piece from its source image sub-rect
    Texture2D CreateShapedTexture(const Piece& piece,
                                  const Image& sourceImage,
                                  int srcX, int srcY,
                                  int cellWPx, int cellHPx) const;

private:
    // Fill a polygon on an image by scanlines (no internal triangle seams)
    static void FillPolygon(Image* img, const std::vector<Vector2>& points, Color color);

    float m_cellW;
    float m_cellH;
    int m_margin;
};

#endif // PIECESHAPEGENERATOR_H
