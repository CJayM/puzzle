#ifndef PLAYSTATE_H
#define PLAYSTATE_H

#include "states/GameState.h"
#include "puzzle/PuzzleGenerator.h"
#include "puzzle/SnapSystem.h"
#include "camera/CameraController.h"
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
    void FitAll();
    void RotateBlock(int blockId);
    void HandleTouchInput(Game& game);
    void MoveBlockTo(int blockId, float newCX, float newCY);

    PuzzleResult m_puzzle;
    std::vector<Block> m_blocks;
    std::unique_ptr<SnapSystem> m_snap;
    std::unique_ptr<CameraController> m_camCtrl;

    int m_draggingBlock = -1;
    int m_selectedBlock = -1;
    Vector2 m_dragOffset = { 0, 0 };
    bool m_wasDragged = false;

    // Touch state
    bool m_touchDragging = false;
    bool m_touchPanning = false;
    int m_touchDragBlock = -1;
    Vector2 m_touchDragOffset = { 0, 0 };
    float m_prevPinchDist = 0.0f;
    int m_prevTouchCount = 0;
};

#endif // PLAYSTATE_H
