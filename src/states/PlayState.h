#ifndef PLAYSTATE_H
#define PLAYSTATE_H

#include "states/GameState.h"
#include "puzzle/PuzzleGenerator.h"

class PlayState : public GameState
{
public:
    void Enter(Game& game) override;
    void Exit(Game& game) override;
    void Update(Game& game) override;
    void Draw(Game& game) override;

private:
    int PickPiece(Vector2 worldPos) const;

    PuzzleResult m_puzzle;
    int m_draggingPiece = -1;      // index of piece being dragged, -1 = none
    Vector2 m_dragOffset = { 0, 0 }; // offset from piece centre to mouse world pos
};

#endif // PLAYSTATE_H
