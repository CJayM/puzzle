#ifndef SNAPSYSTEM_H
#define SNAPSYSTEM_H

#include "puzzle/Piece.h"
#include <vector>

struct SnapCandidate
{
    int pieceA = -1;
    int pieceB = -1;
    int blockA = -1;
    int blockB = -1;
    float snapDist = 0.0f;
};

class SnapSystem
{
public:
    SnapSystem(float cellSize, float snapThreshold);

    SnapCandidate FindSnap(const std::vector<Piece>& pieces,
                           const std::vector<Block>& blocks,
                           int draggedBlockId) const;

    void ApplySnap(std::vector<Piece>& pieces,
                   std::vector<Block>& blocks,
                   const SnapCandidate& snap) const;

private:
    bool AreEdgesCompatible(const Piece& a, const Piece& b,
                            int dgx, int dgy) const;

    float m_cellSize;
    float m_snapThreshold;
};

#endif // SNAPSYSTEM_H
