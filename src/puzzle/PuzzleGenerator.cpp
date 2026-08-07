#include "puzzle/PuzzleGenerator.h"
#include "puzzle/PieceShapeGenerator.h"
#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

static Image GenerateTestImage(int width, int height)
{
    Image img = GenImageColor(width, height, BLACK);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            float fx = (float)x / width, fy = (float)y / height;
            Color c;
            c.r = (unsigned char)((sinf(fx * 3.14159f * 2.0f) * 0.5f + 0.5f) * 255);
            c.g = (unsigned char)((sinf(fy * 3.14159f * 2.0f + 1.0f) * 0.5f + 0.5f) * 255);
            c.b = (unsigned char)((sinf((fx + fy) * 3.14159f * 2.0f + 2.0f) * 0.5f + 0.5f) * 255);
            c.a = 255;
            ImageDrawPixel(&img, x, y, c);
        }
    for (int x = 0; x < width; x += 50) ImageDrawLine(&img, x, 0, x, height, ColorAlpha(WHITE, 0.3f));
    for (int y = 0; y < height; y += 50) ImageDrawLine(&img, 0, y, width, y, ColorAlpha(WHITE, 0.3f));
    ImageDrawCircle(&img, width/2, height/2, width/4, ColorAlpha(RED, 0.5f));
    return img;
}

PuzzleResult PuzzleGenerator::Generate(const std::string& imagePath,
                                       int rows, int cols,
                                       int maxTextureSize)
{
    PuzzleResult result;
    result.rows = rows;
    result.cols = cols;

    Image img = LoadImage(imagePath.c_str());
    if (img.data == nullptr)
    {
        TraceLog(LOG_WARNING, "PuzzleGenerator: failed to load %s, using test pattern", imagePath.c_str());
        img = GenerateTestImage(800, 600);
    }
    TraceLog(LOG_INFO, "PuzzleGenerator: image loaded %dx%d (t=%.2f)", img.width, img.height, GetTime());

    // Compute cell size in pixels
    float cellW = (float)img.width  / (float)cols;
    float cellH = (float)img.height / (float)rows;
    float cellSize = std::min(cellW, cellH);

    // If cells are too small, upscale the image so each cell gets at least ~80 px
    const float MIN_CELL_PX = 80.0f;
    float upscale = 1.0f;
    if (cellSize < MIN_CELL_PX)
        upscale = MIN_CELL_PX / cellSize;

    if (upscale > 1.0f)
    {
        int newW = std::min((int)(img.width * upscale), maxTextureSize);
        int newH = std::min((int)(img.height * upscale), maxTextureSize);
        ImageResize(&img, newW, newH);
        TraceLog(LOG_INFO, "PuzzleGenerator: upscaled image %.1fx (%dx%d)", upscale, newW, newH);
    }

    result.sourceTexture = LoadTextureFromImage(img);
    result.imageWidth  = (float)img.width;
    result.imageHeight = (float)img.height;

    // Recompute cell size after upscale
    cellW = result.imageWidth  / (float)cols;
    cellH = result.imageHeight / (float)rows;
    cellSize = std::min(cellW, cellH);

    float gridPixelW = cellSize * cols;
    float gridPixelH = cellSize * rows;
    result.gridOffsetX = (result.imageWidth  - gridPixelW) / 2.0f;
    result.gridOffsetY = (result.imageHeight - gridPixelH) / 2.0f;
    result.cellSize = cellSize;

    int margin = (int)(cellSize * 0.25f);

    // Create pieces
    result.pieces.reserve(rows * cols);
    for (int r = 0, id = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c, ++id)
        {
            Piece piece;
            piece.id = id;
            piece.gridX = c;
            piece.gridY = r;
            piece.texture = { 0 };
            piece.position = { 0, 0 };
            piece.rotation = 0;
            piece.blockId = id;
            piece.flags = 0;
            result.pieces.push_back(piece);
        }

    // Generate jigsaw edge profiles
    PieceShapeGenerator shapeGen(cellSize, margin);
    shapeGen.GenerateEdgeProfiles(result.pieces, rows, cols);
    TraceLog(LOG_INFO, "PuzzleGenerator: edge profiles done, cell=%.0f margin=%d (t=%.2f)",
             cellSize, margin, GetTime());

    // Create shaped textures for each piece
    float offsetX = result.gridOffsetX;
    float offsetY = result.gridOffsetY;

    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < cols; ++c)
        {
            int idx = r * cols + c;
            int srcX = (int)(offsetX + c * cellSize);
            int srcY = (int)(offsetY + r * cellSize);

            result.pieces[idx].texture = shapeGen.CreateShapedTexture(
                result.pieces[idx], img, srcX, srcY, (int)cellSize);

            // Enable bilinear filtering for smooth edges
            if (result.pieces[idx].texture.id != 0)
                SetTextureFilter(result.pieces[idx].texture, TEXTURE_FILTER_BILINEAR);
        }
        TraceLog(LOG_INFO, "PuzzleGenerator: row %d/%d textures done (t=%.2f)",
                 r + 1, rows, GetTime());
    }

    // Enable bilinear on source texture too
    SetTextureFilter(result.sourceTexture, TEXTURE_FILTER_BILINEAR);

    UnloadImage(img);

    TraceLog(LOG_INFO, "PuzzleGenerator: %d pieces created (%dx%d), cell=%.0fpx",
             rows * cols, cols, rows, cellSize);
    return result;
}

void PuzzleGenerator::ScatterPieces(std::vector<Piece>& pieces,
                                    float scatterRadius, int seed)
{
    if (pieces.empty()) return;
    std::srand(seed != 0 ? seed : 12345);
    for (auto& p : pieces)
    {
        float angle = (float)(std::rand() % 360) * DEG2RAD;
        float dist  = (float)(std::rand() % (int)scatterRadius);
        p.position.x = cosf(angle) * dist;
        p.position.y = sinf(angle) * dist;
    }
}

void PuzzleGenerator::Unload(PuzzleResult& result)
{
    for (auto& p : result.pieces)
        if (p.texture.id != 0) UnloadTexture(p.texture);
    result.pieces.clear();
    if (result.sourceTexture.id != 0) UnloadTexture(result.sourceTexture);
}
