#include "puzzle/SnapSystem.h"
#include "raylib.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

SnapSystem::SnapSystem(float cellW, float cellH, float snapThreshold)
    : m_cellW(cellW)
    , m_cellH(cellH)
    , m_snapThreshold(snapThreshold)
{
}

static Vector2 ExpectedOffset(float cellW, float cellH, int dgx, int dgy, int rotation)
{
    float ox = (float)dgx * cellW;
    float oy = (float)dgy * cellH;

    switch (rotation % 4)
    {
        case 0: return Vector2{ ox, oy };
        case 1: return Vector2{ -oy, ox };
        case 2: return Vector2{ -ox, -oy };
        case 3: return Vector2{ oy, -ox };
        default: return Vector2{ ox, oy };
    }
}

bool SnapSystem::AreEdgesCompatible(const Piece& a, const Piece& b,
                                     int dgx, int dgy) const
{
    const EdgeProfile *edgeA = nullptr, *edgeB = nullptr;

    if (dgx == 1 && dgy == 0)      { edgeA = &a.edges.right;  edgeB = &b.edges.left; }
    else if (dgx == -1 && dgy == 0) { edgeA = &a.edges.left;   edgeB = &b.edges.right; }
    else if (dgx == 0 && dgy == 1)  { edgeA = &a.edges.bottom; edgeB = &b.edges.top; }
    else if (dgx == 0 && dgy == -1) { edgeA = &a.edges.top;    edgeB = &b.edges.bottom; }
    else return false;

    if (edgeA->edgeId < 0 || edgeB->edgeId < 0) return false;
    if (edgeA->edgeId != edgeB->edgeId) return false;
    if (edgeA->sign + edgeB->sign != 0) return false;
    if (edgeA->hash != edgeB->hash) return false;

    return true;
}

SnapCandidate SnapSystem::FindSnap(
    const std::vector<Piece>& pieces,
    const std::vector<Block>& blocks,
    int draggedBlockId) const
{
    SnapCandidate best;
    float bestDist = m_snapThreshold;

    int draggedRotation = 0;
    for (const auto& b : blocks)
        if (b.id == draggedBlockId) { draggedRotation = b.rotation; break; }

    for (const auto& a : pieces)
    {
        if (a.blockId != draggedBlockId) continue;
        if (a.texture.id == 0) continue;

        for (const auto& b : pieces)
        {
            if (b.blockId == draggedBlockId) continue;
            if (b.blockId < 0) continue;
            if (b.texture.id == 0) continue;

            int rotB = 0;
            for (const auto& blk : blocks)
                if (blk.id == b.blockId) { rotB = blk.rotation; break; }
            if (draggedRotation != rotB) continue;

            int dgx = b.gridX - a.gridX;
            int dgy = b.gridY - a.gridY;
            if (std::abs(dgx) + std::abs(dgy) != 1) continue;
            if (!AreEdgesCompatible(a, b, dgx, dgy)) continue;

            Vector2 expOff = ExpectedOffset(m_cellW, m_cellH, dgx, dgy, draggedRotation);
            Vector2 expB = { a.position.x + expOff.x, a.position.y + expOff.y };

            float dx = b.position.x - expB.x;
            float dy = b.position.y - expB.y;
            float dist = sqrtf(dx * dx + dy * dy);

            if (dist < bestDist)
            {
                bestDist = dist;
                best.pieceA = a.id;
                best.pieceB = b.id;
                best.blockA = a.blockId;
                best.blockB = b.blockId;
                best.snapDist = dist;
            }
        }
    }

    return best;
}

void SnapSystem::ApplySnap(std::vector<Piece>& pieces,
                            std::vector<Block>& blocks,
                            const SnapCandidate& snap) const
{
    if (snap.blockA < 0 || snap.blockB < 0 || snap.blockA == snap.blockB)
        return;

    int idxA = -1, idxB = -1;
    for (size_t i = 0; i < blocks.size(); ++i)
    {
        if (blocks[i].id == snap.blockA) idxA = (int)i;
        if (blocks[i].id == snap.blockB) idxB = (int)i;
    }
    if (idxA < 0 || idxB < 0) return;

    Block& blockA = blocks[idxA];
    Block& blockB = blocks[idxB];

    const Piece& pieceA = pieces[snap.pieceA];
    const Piece& pieceB = pieces[snap.pieceB];

    int dgx = pieceB.gridX - pieceA.gridX;
    int dgy = pieceB.gridY - pieceA.gridY;
    Vector2 expOff = ExpectedOffset(m_cellW, m_cellH, dgx, dgy, blockA.rotation);

    Vector2 delta = {
        pieceA.position.x + expOff.x - pieceB.position.x,
        pieceA.position.y + expOff.y - pieceB.position.y
    };

    for (auto& p : pieces)
        if (p.blockId == blockB.id)
            { p.position.x += delta.x; p.position.y += delta.y; }

    blockB.position.x += delta.x;
    blockB.position.y += delta.y;

    for (auto& p : pieces)
        if (p.blockId == blockB.id) p.blockId = blockA.id;

    blockA.pieceIds.insert(blockA.pieceIds.end(),
                           blockB.pieceIds.begin(),
                           blockB.pieceIds.end());

    float minX = FLT_MAX, minY = FLT_MAX, maxX = -FLT_MAX, maxY = -FLT_MAX;
    for (int pid : blockA.pieceIds)
    {
        const auto& p = pieces[pid];
        // Границы блока считаем по клетке, а не по текстуре: текстура шире
        // клетки на поле margin, и область захвата иначе раздувается
        float hw = m_cellW * 0.75f;
        float hh = m_cellH * 0.75f;
        if (p.position.x - hw < minX) minX = p.position.x - hw;
        if (p.position.y - hh < minY) minY = p.position.y - hh;
        if (p.position.x + hw > maxX) maxX = p.position.x + hw;
        if (p.position.y + hh > maxY) maxY = p.position.y + hh;
    }

    blockA.bounds.x = minX;
    blockA.bounds.y = minY;
    blockA.bounds.width = maxX - minX;
    blockA.bounds.height = maxY - minY;
    blockA.position.x = minX + (maxX - minX) / 2.0f;
    blockA.position.y = minY + (maxY - minY) / 2.0f;

    blocks.erase(blocks.begin() + idxB);

    TraceLog(LOG_INFO, "Snap: merged block %d into block %d (%zu pieces)",
             snap.blockB, snap.blockA, blockA.pieceIds.size());
}
