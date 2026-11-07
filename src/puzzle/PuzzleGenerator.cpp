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

// ============================================================
// Подбор сетки под пропорции картинки
// ============================================================
void PuzzleGenerator::ChooseGrid(int baseRows, int baseCols,
                                 int imageW, int imageH,
                                 int& outRows, int& outCols)
{
    const int target = std::max(4, baseRows * baseCols);
    if (imageW <= 0 || imageH <= 0)
    {
        outRows = baseRows;
        outCols = baseCols;
        return;
    }

    const float aspect = (float)imageW / (float)imageH;
    int guess = (int)std::lround(std::sqrt((float)target * aspect));
    guess = std::clamp(guess, 2, 64);

    int bestC = guess;
    int bestR = std::max(2, (int)std::lround((float)target / (float)guess));
    float bestScore = 1e9f;

    // Перебираем столбцы около оценки: клетка aspect*r/c должна быть близка к 1,
    // а произведение r*c — к числу кусочков пресета
    for (int c = std::max(2, guess - 3); c <= guess + 3; ++c)
    {
        const int r = std::max(2, (int)std::lround((float)target / (float)c));
        const float cellAspect = aspect * (float)r / (float)c;
        const float distort = std::fabs(std::logf(cellAspect));
        const float countErr = std::fabs((float)(c * r) - (float)target) / (float)target;
        const float score = distort + countErr;

        if (score < bestScore)
        {
            bestScore = score;
            bestC = c;
            bestR = r;
        }
    }

    outCols = bestC;
    outRows = bestR;
}

PuzzleResult PuzzleGenerator::Generate(const std::string& imagePath,
                                       int rows, int cols,
                                       int maxTextureSize)
{
    PuzzleResult result;

    Image img = LoadImage(imagePath.c_str());
    if (img.data == nullptr)
    {
        TraceLog(LOG_WARNING, "PuzzleGenerator: failed to load %s, using test pattern", imagePath.c_str());
        img = GenerateTestImage(800, 600);
    }
    TraceLog(LOG_INFO, "PuzzleGenerator: image loaded %dx%d (t=%.2f)", img.width, img.height, GetTime());

    // Сетку подбираем под пропорции картинки: число кусочков остаётся близким
    // к пресету, а клетка выходит почти квадратной
    const int baseRows = rows, baseCols = cols;
    {
        int gridRows = rows, gridCols = cols;
        ChooseGrid(baseRows, baseCols, img.width, img.height, gridRows, gridCols);
        rows = gridRows;
        cols = gridCols;
    }
    result.rows = rows;
    result.cols = cols;
    TraceLog(LOG_INFO, "PuzzleGenerator: grid %dx%d (preset %dx%d)",
             cols, rows, baseCols, baseRows);

    // Клетка — ровно ячейка сетки поверх всей картинки: ширина и высота
    // считаются отдельно, поэтому пазл сохраняет пропорции изображения
    float cellW = (float)img.width  / (float)cols;
    float cellH = (float)img.height / (float)rows;

    // If cells are too small, upscale the image so each cell gets at least ~80 px.
    // Множитель один на обе стороны: иначе картинка растянется.
    const float MIN_CELL_PX = 80.0f;
    float upscale = 1.0f;
    const float cellMin = std::min(cellW, cellH);
    if (cellMin < MIN_CELL_PX)
        upscale = MIN_CELL_PX / cellMin;

    if (upscale > 1.0f)
    {
        // Не выходим за maxTextureSize по большей стороне, сохраняя пропорции
        const float limit = (float)maxTextureSize /
                            (float)std::max(img.width, img.height);
        const float s = std::min(upscale, limit);
        if (s > 1.0f)
        {
            const int newW = (int)((float)img.width  * s);
            const int newH = (int)((float)img.height * s);
            ImageResize(&img, newW, newH);
            TraceLog(LOG_INFO, "PuzzleGenerator: upscaled image %.2fx (%dx%d)",
                     s, newW, newH);
        }
    }

    result.sourceTexture = LoadTextureFromImage(img);
    result.imageWidth  = (float)img.width;
    result.imageHeight = (float)img.height;

    // Recompute cell size after upscale (аспект сохранён, клетки прямоугольные)
    cellW = result.imageWidth  / (float)cols;
    cellH = result.imageHeight / (float)rows;

    // Сетка покрывает изображение целиком
    result.gridOffsetX = 0.0f;
    result.gridOffsetY = 0.0f;
    result.cellW = cellW;
    result.cellH = cellH;

    // Поле вокруг клетки: у вытянутой клетки шип по длинной оси выходит за
    // пределы клетки примерно на 0.3 * max(cellW, cellH) — поле считаем от неё
    int margin = (int)(std::max(cellW, cellH) * 0.32f);

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
    PieceShapeGenerator shapeGen(cellW, cellH, margin);
    shapeGen.GenerateEdgeProfiles(result.pieces, rows, cols);
    TraceLog(LOG_INFO, "PuzzleGenerator: edge profiles done, cell=%.0fx%.0f margin=%d (t=%.2f)",
             cellW, cellH, margin, GetTime());

    // Create shaped textures for each piece
    float offsetX = result.gridOffsetX;
    float offsetY = result.gridOffsetY;

    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < cols; ++c)
        {
            int idx = r * cols + c;
            int srcX = (int)(offsetX + c * cellW);
            int srcY = (int)(offsetY + r * cellH);

            result.pieces[idx].texture = shapeGen.CreateShapedTexture(
                result.pieces[idx], img, srcX, srcY, (int)cellW, (int)cellH);

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

    TraceLog(LOG_INFO, "PuzzleGenerator: %d pieces created (%dx%d), cell=%.0fx%.0fpx",
             rows * cols, cols, rows, cellW, cellH);
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
