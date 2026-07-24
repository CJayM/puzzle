#include "puzzle/PieceShapeGenerator.h"
#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

// --- Simple deterministic hash (xorshift) ---
static unsigned int xorshift32(unsigned int state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

static float randFloat(unsigned int& state, float min, float max)
{
    state = xorshift32(state);
    return min + (max - min) * (state / 4294967296.0f);
}

// ============================================================
// Constructor
// ============================================================
PieceShapeGenerator::PieceShapeGenerator(float cellSize, int margin)
    : m_cellSize(cellSize)
    , m_margin(margin)
{
}

// ============================================================
// Generate a unique profile for an edge from its ID
// ============================================================
EdgeProfile PieceShapeGenerator::GenerateProfile(int edgeId, bool vertical)
{
    EdgeProfile p;
    p.edgeId = edgeId;
    p.sign = (edgeId % 2 == 0) ? +1 : -1;

    unsigned int state = (unsigned int)(edgeId * 2654435761u);
    state = xorshift32(state);

    p.tabLength  = randFloat(state, 0.06f, 0.12f);   // wave amplitude (fraction of edge length)
    p.tabWidth   = randFloat(state, 0.0f,  0.5f);    // controls wave periods (2 + 3*val → 2..3.5)
    p.neckWidth  = randFloat(state, 0.3f,  0.5f);
    p.curvatureA = randFloat(state, 0.3f,  0.7f);
    p.curvatureB = randFloat(state, 0.3f,  0.7f);
    p.asymmetry  = randFloat(state, -0.5f, 0.5f);    // phase shift
    p.jitter     = randFloat(state, 0.0025f, 0.01f);   // noise amplitude
    p.seed       = state;

    // Simple hash
    p.hash = (uint64_t)edgeId * 0x9E3779B97F4A7C15ull;
    p.hash ^= (uint64_t)(p.tabLength  * 1000);
    p.hash ^= (uint64_t)(p.tabWidth   * 1000) << 8;
    p.hash ^= (uint64_t)(p.neckWidth  * 1000) << 16;
    p.hash ^= (uint64_t)(p.asymmetry  * 1000) << 24;

    return p;
}

// ============================================================
// Assign unique edge profiles to all internal edges in the grid
// ============================================================
void PieceShapeGenerator::GenerateEdgeProfiles(std::vector<Piece>& pieces,
                                                int rows, int cols,
                                                unsigned int seed)
{
    if (pieces.empty()) return;

    // We assign edgeIds to each unique internal edge:
    // Horizontal edges (between rows): id range [0, (rows-1)*cols)
    // Vertical edges (between cols):   id range [base, base + rows*(cols-1))
    int horzBase = 0;
    int vertBase = (rows - 1) * cols;
    int edgeCount = vertBase + rows * (cols - 1);

    // Array of profiles per edge ID
    std::vector<EdgeProfile> profiles(edgeCount);
    for (int i = 0; i < edgeCount; ++i)
    {
        bool vertical = (i >= vertBase);
        profiles[i] = GenerateProfile(i + seed, vertical);
    }

    // Assign profiles to pieces
    for (auto& p : pieces)
    {
        int gx = p.gridX;
        int gy = p.gridY;

        // Top edge (horizontal, between p and piece above)
        if (gy > 0)
        {
            int eid = horzBase + (gy - 1) * cols + gx;
            p.edges.top = profiles[eid];
            // For the top edge of a piece, the sign depends on whether the
            // profile was generated as Tab (sign=+1, pointing up for top edge).
            // The top edge's outward direction is -Y (up).
            // Tab means the edge bulges upward (outward).
            // Blank means it bulges downward (inward).
            // The profile's sign already encodes this.
        }
        else
        {
            p.edges.top = EdgeProfile{ -1, 0 }; // Flat (border)
        }

        // Bottom edge
        if (gy < rows - 1)
        {
            int eid = horzBase + gy * cols + gx;
            auto& prof = profiles[eid];
            // Bottom edge of piece shares the same edgeId as top edge of piece below.
            // Sign is flipped: if the piece below has Tab (bulging down into this piece's
            // cell), then this piece's bottom edge should be Blank.
            p.edges.bottom = prof;
            p.edges.bottom.sign = -prof.sign;
        }
        else
        {
            p.edges.bottom = EdgeProfile{ -1, 0 };
        }

        // Left edge
        if (gx > 0)
        {
            int eid = vertBase + gy * (cols - 1) + (gx - 1);
            p.edges.left = profiles[eid];
        }
        else
        {
            p.edges.left = EdgeProfile{ -1, 0 };
        }

        // Right edge
        if (gx < cols - 1)
        {
            int eid = vertBase + gy * (cols - 1) + gx;
            auto& prof = profiles[eid];
            p.edges.right = prof;
            p.edges.right.sign = -prof.sign;
        }
        else
        {
            p.edges.right = EdgeProfile{ -1, 0 };
        }
    }
}

// ============================================================
// Generate polyline points for one edge with organic wavy shape
// ============================================================
std::vector<Vector2> PieceShapeGenerator::GenerateEdgePoints(
    Vector2 start, Vector2 end,
    Vector2 outward,
    const EdgeProfile& profile,
    int segments)
{
    std::vector<Vector2> points;
    if (segments < 2) segments = 2;

    Vector2 dir = { end.x - start.x, end.y - start.y };
    float edgeLen = sqrtf(dir.x * dir.x + dir.y * dir.y);
    if (edgeLen < 0.001f)
    {
        points.push_back(start);
        points.push_back(end);
        return points;
    }
    dir.x /= edgeLen;
    dir.y /= edgeLen;

    // Flat edge (border)
    if (profile.edgeId < 0 || profile.sign == 0)
    {
        points.push_back(start);
        points.push_back(end);
        return points;
    }

    // Organic wavy edge: sine wave + noise along the entire edge,
    // with a Gaussian tab/blank bump added at the centre
    float amplitude  = profile.tabLength * edgeLen;       // overall wave amplitude
    float periods    = 2.0f + 3.0f * profile.tabWidth;    // number of waves (2-3.5)
    float phaseShift = profile.asymmetry * 6.2832f;       // phase offset
    float noiseAmp   = profile.jitter * edgeLen;          // micro-roughness
    periods = (periods - 1.9f) * 0.1f;
    noiseAmp = 0.05f;
    phaseShift = phaseShift * 0.04f;

    // Gaussian bump (localised tab / blank)
    float bumpAmp = profile.curvatureA * edgeLen * 0.3f; // bump height
    float bumpWidth = profile.neckWidth * 0.5f;          // width fraction

    for (int i = 0; i <= segments; ++i)
    {
        float t = (float)i / segments;                    // 0..1 along the edge
        float posAlong = t * edgeLen;

        // Window function: 0 at ends, 1 at centre — ensures corners always meet
        float window = 4.0f * t * (1.0f - t);             // parabola peaking at 1

        // Sine wave
        float wave = sinf(posAlong * periods * 6.2832f / edgeLen + phaseShift);

        // Simple hash-based noise (reproducible per edge)
        float nHash = sinf(t * 0.794f + profile.seed * 0.5f) * 43758.5453f;
        float noise = nHash - floorf(nHash);               // 0..1
        noise = noise * 2.0f - 1.0f;                       // -1..1

        // Gaussian bump: light-bulb shape centred at t=0.5
        float dist = (t - 0.5f) / bumpWidth;
        float gauss = 0.0f;
        if (std::abs(dist) <= 1.0f)
        {
            float dsq = dist * dist;
            gauss = expf(-dsq * 4.0f) * (1.0f + 3.0f * dsq);
        }

        // Combined = organic wave + localised bump
        float displacement = (float)profile.sign * amplitude *
                             (wave + noiseAmp * noise) * window
                           + (float)profile.sign * bumpAmp * gauss;

        Vector2 pos = {
            start.x + dir.x * posAlong + outward.x * displacement,
            start.y + dir.y * posAlong + outward.y * displacement
        };
        points.push_back(pos);
    }

    return points;
}

// ============================================================
// Build the full polygon outline for a piece
// ============================================================
std::vector<Vector2> PieceShapeGenerator::BuildOutline(
    const Piece& piece, float cellSize) const
{
    const int SEG = 24;
    std::vector<Vector2> outline;

    float cs = cellSize;

    // We trace clockwise but generate each edge in a consistent spatial
    // direction (left→right for horizontal, top→bottom for vertical)
    // so complementary edges between pieces match exactly.

    // Top edge: trace left→right as-is for clockwise outline
    {
        auto pts = GenerateEdgePoints(
            { 0, 0 }, { cs, 0 }, { 0, -1 },
            piece.edges.top, SEG);
        outline.insert(outline.end(), pts.begin(), pts.end());
        outline.pop_back();
    }

    // Right edge: trace top→bottom, points go clockwise
    {
        auto pts = GenerateEdgePoints(
            { cs, 0 }, { cs, cs }, { 1, 0 },
            piece.edges.right, SEG);
        outline.insert(outline.end(), pts.begin(), pts.end());
        outline.pop_back();
    }

    // Bottom edge: generate left→right to match neighbour's top edge,
    // then REVERSE so the outline stays clockwise
    {
        auto pts = GenerateEdgePoints(
            { 0, cs }, { cs, cs }, { 0, 1 },
            piece.edges.bottom, SEG);
        std::reverse(pts.begin(), pts.end());
        outline.insert(outline.end(), pts.begin(), pts.end());
        outline.pop_back();
    }

    // Left edge: generate top→bottom to match neighbour's right edge,
    // then REVERSE for clockwise outline
    {
        auto pts = GenerateEdgePoints(
            { 0, 0 }, { 0, cs }, { -1, 0 },
            piece.edges.left, SEG);
        std::reverse(pts.begin(), pts.end());
        outline.insert(outline.end(), pts.begin(), pts.end());
        // no pop_back — closes the polygon
    }

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
    if (maxY >= img->height) maxY = img->height - 1;
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
            if (x2 >= img->width) x2 = img->width - 1;

            for (int x = x1; x <= x2; ++x)
            {
                ImageDrawPixel(img, x, y, color);
            }
        }
    }
}

// ============================================================
// Create a shaped texture with alpha mask
// ============================================================
Texture2D PieceShapeGenerator::CreateShapedTexture(
    const Piece& piece,
    const Image& sourceImage,
    int srcX, int srcY,
    int cellSizePx) const
{
    int texSize = cellSizePx + m_margin * 2;

    // Extract a larger region from the source that includes the margin area
    // so tabs extending beyond the cell show the correct image content.
    int extX = srcX - m_margin;
    int extY = srcY - m_margin;
    int extW = texSize;
    int extH = texSize;

    // Clamp to source image bounds — areas outside are filled transparent
    int clampX = (extX < 0) ? 0 : extX;
    int clampY = (extY < 0) ? 0 : extY;
    int clampW = extW;
    int clampH = extH;
    if (clampX + clampW > sourceImage.width)  clampW = sourceImage.width  - clampX;
    if (clampY + clampH > sourceImage.height) clampH = sourceImage.height - clampY;

    // Create the extended piece texture from the source
    Image extended = GenImageColor(texSize, texSize, BLANK);

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

    // Build the polygon outline in texture coordinates (0,0) = top-left of texture
    std::vector<Vector2> polygon = BuildOutline(piece, (float)cellSizePx);

    // Polygon is built in cell-local space where the cell is (0,0)-(cellSize,cellSize).
    // We need to offset by margin so the polygon aligns with the texture.
    for (auto& p : polygon)
        { p.x += m_margin; p.y += m_margin; }

    // Create alpha mask
    Image mask = GenImageColor(texSize, texSize, BLANK);
    FillPolygon(&mask, polygon, WHITE);
    ImageFormat(&mask, PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA);

    // Apply alpha mask
    ImageAlphaMask(&extended, mask);
    UnloadImage(mask);

    Texture2D tex = LoadTextureFromImage(extended);
    UnloadImage(extended);

    return tex;
}
