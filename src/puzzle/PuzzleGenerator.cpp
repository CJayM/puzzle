#include "puzzle/PuzzleGenerator.h"
#include "puzzle/PieceShapeGenerator.h"
#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

// Generate a colorful test image when the real image can't be loaded
static Image GenerateTestImage(int width, int height)
{
    Image img = GenImageColor(width, height, BLACK);

    // Draw colorful gradient bands
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            float fx = (float)x / width;
            float fy = (float)y / height;

            Color c;
            c.r = (unsigned char)((std::sin(fx * 3.14159f * 2.0f) * 0.5f + 0.5f) * 255);
            c.g = (unsigned char)((std::sin(fy * 3.14159f * 2.0f + 1.0f) * 0.5f + 0.5f) * 255);
            c.b = (unsigned char)((std::sin((fx + fy) * 3.14159f * 2.0f + 2.0f) * 0.5f + 0.5f) * 255);
            c.a = 255;

            ImageDrawPixel(&img, x, y, c);
        }
    }

    // Draw grid lines
    for (int x = 0; x < width; x += 50)
        ImageDrawLine(&img, x, 0, x, height, ColorAlpha(WHITE, 0.3f));
    for (int y = 0; y < height; y += 50)
        ImageDrawLine(&img, 0, y, width, y, ColorAlpha(WHITE, 0.3f));

    // Draw a circle
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

    // Load and optionally downscale the image
    Image img = LoadImage(imagePath.c_str());
    if (img.data == nullptr)
    {
        TraceLog(LOG_WARNING, "PuzzleGenerator: failed to load %s, using test pattern",
                 imagePath.c_str());
        img = GenerateTestImage(800, 600);
    }

    // Downscale if needed
    if (img.width > maxTextureSize || img.height > maxTextureSize)
    {
        float scale = (float)maxTextureSize / (float)std::max(img.width, img.height);
        ImageResize(&img, (int)(img.width * scale), (int)(img.height * scale));
    }

    // Create source texture
    result.sourceTexture = LoadTextureFromImage(img);

    result.imageWidth  = (float)img.width;
    result.imageHeight = (float)img.height;

    // Calculate cell size (square cells that fit the image)
    float cellW = result.imageWidth  / (float)cols;
    float cellH = result.imageHeight / (float)rows;
    float cellSize = std::min(cellW, cellH);

    // Margin for jigsaw tabs (25% of cell size)
    int margin = (int)(cellSize * 0.25f);

    // Center the grid on the image
    float gridPixelW = cellSize * cols;
    float gridPixelH = cellSize * rows;
    float offsetX = (result.imageWidth  - gridPixelW) / 2.0f;
    float offsetY = (result.imageHeight - gridPixelH) / 2.0f;

    result.cellSize = cellSize;

    // Create pieces (initially flat)
    int texSize = (int)cellSize + margin * 2;
    result.pieces.reserve(rows * cols);
    int id = 0;

    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < cols; ++c)
        {
            Piece piece;
            piece.id = id;
            piece.gridX = c;
            piece.gridY = r;
            piece.texture = { 0 };  // will be set after shape generation
            piece.position = { 0, 0 };
            piece.rotation = 0;
            piece.blockId = id;
            piece.flags = 0;
            piece.localBounds = { (float)-margin, (float)-margin,
                                  (float)texSize, (float)texSize };

            result.pieces.push_back(piece);
            ++id;
        }
    }

    // Generate jigsaw edge profiles
    PieceShapeGenerator shapeGen(cellSize, margin);
    shapeGen.GenerateEdgeProfiles(result.pieces, rows, cols);

    // Create shaped textures for each piece
    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < cols; ++c)
        {
            int idx = r * cols + c;

            int srcX = (int)(offsetX + c * cellSize);
            int srcY = (int)(offsetY + r * cellSize);

            result.pieces[idx].texture = shapeGen.CreateShapedTexture(
                result.pieces[idx], img, srcX, srcY, (int)cellSize);
        }
    }

    UnloadImage(img);

    TraceLog(LOG_INFO, "PuzzleGenerator: %d jigsaw pieces created (%dx%d)",
             id, cols, rows);
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
        p.position.x = std::cos(angle) * dist;
        p.position.y = std::sin(angle) * dist;
    }
}

void PuzzleGenerator::Unload(PuzzleResult& result)
{
    for (auto& p : result.pieces)
    {
        if (p.texture.id != 0)
            UnloadTexture(p.texture);
    }
    result.pieces.clear();

    if (result.sourceTexture.id != 0)
        UnloadTexture(result.sourceTexture);
}
