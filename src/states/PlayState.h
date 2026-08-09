#ifndef PLAYSTATE_H
#define PLAYSTATE_H

#include "states/GameState.h"
#include "puzzle/PuzzleGenerator.h"
#include "puzzle/SnapSystem.h"
#include "camera/CameraController.h"
#include "ui/Button.h"
#include <memory>

struct SnapAnimation
{
    bool active = false;
    int movingBlockId = -1;
    int targetBlockId = -1;
    SnapCandidate snap;
    std::vector<int> pieceIds;         // pieces in the moving block
    std::vector<Vector2> startPos;     // their start positions
    Vector2 delta = { 0, 0 };          // total displacement
    float elapsed = 0.0f;
    float duration = 0.15f;
};

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
    void TryStartSnap(int draggedBlockId);
    void CompleteSnap();
    void CreatePieceMeshes();

    PuzzleResult m_puzzle;
    std::vector<Block> m_blocks;
    std::vector<Mesh> m_meshes;          // one mesh per piece (polygon with UV)
    Material m_material = { 0 };          // shared material with source texture
    std::unique_ptr<SnapSystem> m_snap;
    std::unique_ptr<CameraController> m_camCtrl;

    int m_draggingBlock = -1;
    int m_selectedBlock = -1;
    Vector2 m_dragOffset = { 0, 0 };
    bool m_wasDragged = false;

    // Snap animation
    SnapAnimation m_snapAnim;

    // Timer
    float m_elapsedTime = 0.0f;
    float m_lastAutoSave = 0.0f; // periodic autosave timer (orientation changes)
    bool m_wireframe = false;

    // Sound
    Sound m_clickSound = { 0 };

    // Touch state
    bool m_touchDragging = false;
    bool m_touchPanning = false;
    int m_touchDragBlock = -1;
    Vector2 m_touchDragOffset = { 0, 0 };
    Vector2 m_prevTouchPos = { 0, 0 };
    float m_prevPinchDist = 0.0f;
    int m_prevTouchCount = 0;

    // "Eye" hint — shows the solved image while held
    bool m_showSolvedHint = false;

    // HUD: layout and buttons are created once (Enter) and re-computed
    // every frame (window size / label may change)
    ui::HLayout m_hudLayout;
    ui::Spacer m_hudSpacer;
    ui::Button m_btnZoomIn, m_btnZoomOut, m_btnReset, m_btnFit;
    ui::Button m_btnBg, m_btnMenu, m_btnEye;

    void InitHud();   // create buttons + layout (Enter)
    void UpdateHud(); // refresh label and recompute positions (every frame)
};

#endif // PLAYSTATE_H
