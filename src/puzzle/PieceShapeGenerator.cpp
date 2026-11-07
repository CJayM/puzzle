#include "puzzle/PieceShapeGenerator.h"
#include "puzzle/ClassicOutline.h"
#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

// ============================================================
// Constructor
// ============================================================
PieceShapeGenerator::PieceShapeGenerator(float cellW, float cellH, int margin)
    : m_cellW(cellW)
    , m_cellH(cellH)
    , m_margin(margin)
{
}

namespace {

// Профиль одного ребра сетки: плоский у границы картинки, иначе общий для
// двух соседних деталей (одинаковые edgeId и hash — по ним SnapSystem
// проверяет, что стыкуются именно совместимые края)
EdgeProfile MakeEdge(bool interior, int sign, int edgeId)
{
    if (!interior) return EdgeProfile{};

    EdgeProfile p;
    p.edgeId = edgeId;
    p.sign = sign;
    p.hash = (uint64_t)(unsigned int)edgeId * 0x9E3779B97F4A7C15ull;
    return p;
}

} // namespace

// ============================================================
// Assign edge types to all edges of the grid
// ============================================================
// Классическая раскладка (piece_maker.py): тип края зависит только от
// чётности (row + col), поэтому соседние детали всегда комплементарны —
//   чётная деталь:   низ шип, лево паз,  верх шип,  право паз
//   нечётная деталь: низ паз,  лево шип, верх паз,  право шип
void PieceShapeGenerator::GenerateEdgeProfiles(std::vector<Piece>& pieces,
                                                int rows, int cols,
                                                unsigned int /*seed*/)
{
    if (pieces.empty()) return;

    const int horzBase = 0;                 // рёбра между строками
    const int vertBase = (rows - 1) * cols; // рёбра между столбцами

    for (auto& p : pieces)
    {
        const int gx = p.gridX;
        const int gy = p.gridY;
        const bool even = ((gx + gy) % 2 == 0);

        p.edges.bottom = MakeEdge(gy < rows - 1, even ? +1 : -1,
                                  horzBase + gy * cols + gx);
        p.edges.top    = MakeEdge(gy > 0, even ? +1 : -1,
                                  horzBase + (gy - 1) * cols + gx);
        p.edges.left   = MakeEdge(gx > 0, even ? -1 : +1,
                                  vertBase + gy * (cols - 1) + (gx - 1));
        p.edges.right  = MakeEdge(gx < cols - 1, even ? -1 : +1,
                                  vertBase + gy * (cols - 1) + gx);
    }
}

// ============================================================
// Build the full polygon outline for a piece
// ============================================================
std::vector<Vector2> PieceShapeGenerator::BuildOutline(
    const Piece& piece, float cellW, float cellH) const
{
    auto kind = [](const EdgeProfile& e)
    {
        if (e.sign > 0) return ClassicEdge::Tab;
        if (e.sign < 0) return ClassicEdge::Blank;
        return ClassicEdge::Flat;
    };

    // Шагов на кривую — по меньшей стороне клетки: мелким деталям плотная
    // выборка не нужна, крупные без неё выглядят гранёными
    const int steps = std::clamp((int)(std::min(cellW, cellH) * 0.2f), 10, 48);

    // Ключ шума: у внутреннего ребра — его edgeId (один и тот же у двух
    // соседних деталей, поэтому стык остаётся точным), у края картинки —
    // собственный ключ детали, чтобы внешние края тоже были разными
    auto edgeKey = [&piece](const EdgeProfile& e, int side) -> unsigned
    {
        if (e.edgeId >= 0) return (unsigned)e.edgeId * 2654435761u + 12345u;
        unsigned h = (unsigned)(piece.gridX * 73856093)
                   ^ (unsigned)(piece.gridY * 19349663)
                   ^ (unsigned)((side + 1) * 83492791);
        return h * 2654435761u + 777u;
    };

    ClassicEdgeKeys keys;
    keys.key[0] = edgeKey(piece.edges.bottom, 0);
    keys.key[1] = edgeKey(piece.edges.left, 1);
    keys.key[2] = edgeKey(piece.edges.top, 2);
    keys.key[3] = edgeKey(piece.edges.right, 3);

    // Небольшой шум Перлина (доля длины края) делает каждую деталь уникальной
    constexpr float NOISE_AMPLITUDE = 0.06f;

    std::vector<Vector2> outline = BuildClassicOutline(
        cellW, cellH,
        kind(piece.edges.bottom), kind(piece.edges.left),
        kind(piece.edges.top), kind(piece.edges.right),
        steps, &keys, NOISE_AMPLITUDE);

    // Контур строится вокруг центра клетки, текстура — от её левого верхнего угла
    const float halfW = cellW * 0.5f;
    const float halfH = cellH * 0.5f;
    for (auto& p : outline) { p.x += halfW; p.y += halfH; }

    return outline;
}

// ============================================================
// Scanline fill a polygon on an image (no internal triangle edges)
// ============================================================
void PieceShapeGenerator::FillPolygon(Image* img,
                                       const std::vector<Vector2>& points,
                                       Color color)
{
    if (points.size() < 3) return;

    // Direct RGBA8 pixel access (mask is created with GenImageColor)
    unsigned char* data = (unsigned char*)img->data;
    int w = img->width;
    int h = img->height;

    // Find vertical bounds
    int minY = 1000000, maxY = -1;
    for (const auto& p : points)
    {
        int py = (int)p.y;
        if (py < minY) minY = py;
        if (py > maxY) maxY = py;
    }

    // Clamp to image bounds
    if (minY < 0) minY = 0;
    if (maxY >= h) maxY = h - 1;
    if (minY > maxY) return;

    int n = (int)points.size();

    // For each scanline
    for (int y = minY; y <= maxY; ++y)
    {
        float fy = (float)y + 0.5f;

        // Collect intersections with all edges
        std::vector<float> xs;
        for (int i = 0; i < n; ++i)
        {
            int j = (i + 1) % n;
            float y1 = points[i].y;
            float y2 = points[j].y;

            // Skip horizontal edges and edges that don't cross this scanline
            if ((y1 < fy && y2 < fy) || (y1 >= fy && y2 >= fy))
                continue;

            // Edge crosses the scanline — compute x at y
            float t = (fy - y1) / (y2 - y1);
            if (t >= 0.0f && t <= 1.0f)
            {
                float x = points[i].x + t * (points[j].x - points[i].x);
                xs.push_back(x);
            }
        }

        // Sort intersections
        std::sort(xs.begin(), xs.end());

        // Fill pairs
        for (size_t k = 0; k + 1 < xs.size(); k += 2)
        {
            int x1 = (int)xs[k];
            int x2 = (int)xs[k + 1];

            // Clamp to image
            if (x1 < 0) x1 = 0;
            if (x2 >= w) x2 = w - 1;

            for (int x = x1; x <= x2; ++x)
            {
                int idx = (y * w + x) * 4;
                data[idx + 0] = color.r;
                data[idx + 1] = color.g;
                data[idx + 2] = color.b;
                data[idx + 3] = color.a;
            }
        }
    }
}

// ============================================================
// Alpha mask of the piece shape
// ============================================================
Image PieceShapeGenerator::BuildPieceMask(const Piece& piece, int cellWPx, int cellHPx) const
{
    const int texW = cellWPx + m_margin * 2;
    const int texH = cellHPx + m_margin * 2;

    // Контур в координатах клетки (0,0) = левый верхний угол,
    // сдвигаем на margin — маска покрывает и выступающие шипы
    std::vector<Vector2> polygon = BuildOutline(piece, (float)cellWPx, (float)cellHPx);
    for (auto& p : polygon) { p.x += m_margin; p.y += m_margin; }

    // Жёсткие 0/255: сглаживание краёв делает MSAA при отрисовке
    Image mask = GenImageColor(texW, texH, BLANK);
    FillPolygon(&mask, polygon, WHITE);
    ImageFormat(&mask, PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA);
    return mask;
}

// ============================================================
// Create a shaped texture with alpha mask
// ============================================================
Texture2D PieceShapeGenerator::CreateShapedTexture(
    const Piece& piece,
    const Image& sourceImage,
    int srcX, int srcY,
    int cellWPx, int cellHPx) const
{
    const int texW = cellWPx + m_margin * 2;
    const int texH = cellHPx + m_margin * 2;

    // Extract a larger region from the source that includes the margin area
    // so tabs extending beyond the cell show the correct image content.
    int extX = srcX - m_margin;
    int extY = srcY - m_margin;
    int extW = texW;
    int extH = texH;

    // Clamp to source image bounds — areas outside are filled transparent
    int clampX = (extX < 0) ? 0 : extX;
    int clampY = (extY < 0) ? 0 : extY;
    int clampW = extW;
    int clampH = extH;
    if (clampX + clampW > sourceImage.width)  clampW = sourceImage.width  - clampX;
    if (clampY + clampH > sourceImage.height) clampH = sourceImage.height - clampY;

    // Create the extended piece texture from the source
    Image extended = GenImageColor(texW, texH, BLANK);

    if (clampW > 0 && clampH > 0)
    {
        Image sub = ImageFromImage(sourceImage,
            Rectangle{ (float)clampX, (float)clampY, (float)clampW, (float)clampH });
        int dstX = clampX - extX;
        int dstY = clampY - extY;
        ImageDraw(&extended, sub,
                  Rectangle{ 0, 0, (float)clampW, (float)clampH },
                  Rectangle{ (float)dstX, (float)dstY, (float)clampW, (float)clampH },
                  WHITE);
        UnloadImage(sub);
    }

    // If no jigsaw edges, return the rectangular texture as-is
    if (piece.edges.top.edgeId < 0 && piece.edges.bottom.edgeId < 0 &&
        piece.edges.left.edgeId < 0 && piece.edges.right.edgeId < 0)
    {
        Texture2D tex = LoadTextureFromImage(extended);
        UnloadImage(extended);
        return tex;
    }

    // Build the mask in texture coordinates and apply it
    Image mask = BuildPieceMask(piece, cellWPx, cellHPx);
    ImageAlphaMask(&extended, mask);
    UnloadImage(mask);

    Texture2D tex = LoadTextureFromImage(extended);
    UnloadImage(extended);

    return tex;
}
