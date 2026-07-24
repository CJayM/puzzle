#ifndef PLAYSTATE_H
#define PLAYSTATE_H

#include "states/GameState.h"
#include "puzzle/PuzzleGenerator.h"
#include "puzzle/SnapSystem.h"
#include <memory>

class PlayState : public GameState
{
public:
    void Enter(Game& game) override;
    void Exit(Game& game) override;
    void Update(Game& game) override;
    void Draw(Game& game) override;

private:
    int PickBlock(Vector2 worldPos) const;

    PuzzleResult m_puzzle;
    std::vector<Block> m_blocks;
    std::unique_ptr<SnapSystem> m_snap;

    int m_draggingBlock = -1;  // block id being dragged, -1 = none
    Vector2 m_dragOffset = { 0, 0 };
    bool m_wasDragged = false; // was the block actually moved this drag?
};

#endif // PLAYSTATE_H
