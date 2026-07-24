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
    void FitAll(Game& game);
    void RotateBlock(int blockId, Game& game);

    PuzzleResult m_puzzle;
    std::vector<Block> m_blocks;
    std::unique_ptr<SnapSystem> m_snap;

    int m_draggingBlock = -1;
    int m_selectedBlock = -1; // last clicked block (for rotation when not dragging)
    Vector2 m_dragOffset = { 0, 0 };
    bool m_wasDragged = false;
};

#endif // PLAYSTATE_H
