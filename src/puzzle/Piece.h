#ifndef PIECE_H
#define PIECE_H

#include "raylib.h"
#include <cstdint>
#include <vector>

// --- Edge types ---
enum class EdgeType { Flat, Tab, Blank };

// --- Profile of one edge of a piece ---
// Форма края классическая (см. ClassicOutline.h), поэтому от профиля нужна
// только идентичность ребра сетки и его полярность: +1 — шип наружу,
// -1 — паз внутрь, 0 — ровный край картинки.
struct EdgeProfile
{
    int edgeId = -1;    // -1 = border (flat); shared between mating edges
    int sign = 0;       // +1 = Tab (bulge outward), -1 = Blank, 0 = Flat
    uint64_t hash = 0;  // identity of the shared edge (snap check)
};

// --- Four edges of one piece ---
struct PieceEdges
{
    EdgeProfile top;
    EdgeProfile right;
    EdgeProfile bottom;
    EdgeProfile left;
};

// --- Piece state flags ---
enum PieceFlag : uint32_t
{
    PIECE_NONE     = 0,
    PIECE_LOCKED   = (1u << 0)   // part of a multi-piece block (not individually draggable)
};

// --- A single puzzle piece ---
struct Piece
{
    int id = -1;

    int gridX = 0;      // correct column in original grid
    int gridY = 0;      // correct row in original grid

    Texture2D texture = { 0 };

    Vector2 position = { 0, 0 };  // world position of the piece centre
    int rotation = 0;             // 0,1,2,3 → 0°,90°,180°,270°

    int blockId = -1;   // which Block this piece belongs to (-1 = standalone)

    PieceEdges edges;
    uint32_t flags = 0;

    Rectangle localBounds = { 0, 0, 0, 0 }; // bounding box in local coords (before rotation)
};

// --- A mergeable block (one or more correctly joined pieces) ---
struct Block
{
    int id = -1;
    std::vector<int> pieceIds;  // indices into the global pieces array

    Vector2 position = { 0, 0 }; // world centre of the block
    int rotation = 0;             // 0,1,2,3

    Rectangle bounds = { 0, 0, 0, 0 }; // world-space AABB (after rotation)
};

#endif // PIECE_H
