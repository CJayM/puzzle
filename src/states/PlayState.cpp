#include "states/PlayState.h"
#include "states/CompletedState.h"
#include "states/MenuState.h"
#include "core/Game.h"
#include "core/Config.h"
#include "raylib.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

// ---- HUD button helpers ----
static Rectangle btnRect(float x, float y, float w, float h)
{
    return Rectangle{ x, y, w, h };
}

static bool btnHover(Rectangle r)
{
    Vector2 m = GetMousePosition();
    return CheckCollisionPointRec(m, r);
}

static void drawBtn(Rectangle r, const char* label, Color bg, bool enabled)
{
    Color c = enabled ? bg : ColorAlpha(bg, 0.3f);
    DrawRectangleRec(r, c);
    DrawRectangleLinesEx(r, 1, enabled ? ColorAlpha(WHITE, 0.6f) : ColorAlpha(WHITE, 0.2f));
    DrawText(label, (int)r.x + 6, (int)r.y + 6, 14, enabled ? WHITE : GRAY);
}

// ============================================================
// Enter
// ============================================================
void PlayState::Enter(Game& game)
{
    const auto& images = game.GetResources().GetImageList();

    if (game.selectedImageIndex < 0 ||
        game.selectedImageIndex >= (int)images.size())
    {
        TraceLog(LOG_ERROR, "PlayState: invalid image index %d", game.selectedImageIndex);
        return;
    }

    const std::string& path = images[game.selectedImageIndex].filePath;

    PuzzleGenerator gen;
    m_puzzle = gen.Generate(path, game.selectedRows, game.selectedCols);

    if (m_puzzle.pieces.empty())
    {
        TraceLog(LOG_ERROR, "PlayState: no pieces generated");
        return;
    }

    float snapThreshold = Config::Instance().snapDistanceFactor * m_puzzle.cellSize;
    m_snap = std::make_unique<SnapSystem>(m_puzzle.cellSize, snapThreshold);

    // One block per piece
    m_blocks.clear();
    m_blocks.reserve(m_puzzle.pieces.size());
    for (auto& p : m_puzzle.pieces)
    {
        Block block;
        block.id = p.blockId;
        block.pieceIds = { p.id };
        block.position = p.position;
        block.rotation = 0;
        float hw = (float)p.texture.width / 2.0f;
        float hh = (float)p.texture.height / 2.0f;
        block.bounds.x = p.position.x - hw;
        block.bounds.y = p.position.y - hh;
        block.bounds.width = (float)p.texture.width;
        block.bounds.height = (float)p.texture.height;
        m_blocks.push_back(block);
    }

    // Scatter
    float scatterRadius = m_puzzle.cellSize * (float)std::max(m_puzzle.cols, m_puzzle.rows) * 1.5f;
    PuzzleGenerator::ScatterPieces(m_puzzle.pieces, scatterRadius);

    for (auto& p : m_puzzle.pieces)
        for (auto& b : m_blocks)
            if (b.id == p.blockId)
            {
                b.position = p.position;
                float minX = FLT_MAX, minY = FLT_MAX, maxX = -FLT_MAX, maxY = -FLT_MAX;
                for (int pid : b.pieceIds)
                {
                    const auto& pp = m_puzzle.pieces[pid];
                    float hw = (float)pp.texture.width / 2.0f;
                    float hh = (float)pp.texture.height / 2.0f;
                    if (pp.position.x - hw < minX) minX = pp.position.x - hw;
                    if (pp.position.y - hh < minY) minY = pp.position.y - hh;
                    if (pp.position.x + hw > maxX) maxX = pp.position.x + hw;
                    if (pp.position.y + hh > maxY) maxY = pp.position.y + hh;
                }
                b.bounds.x = minX; b.bounds.y = minY;
                b.bounds.width = maxX - minX; b.bounds.height = maxY - minY;
                break;
            }

    // Reset camera
    auto& cfg = Config::Instance();
    m_camCtrl = std::make_unique<CameraController>(game.GetCamera());
    m_camCtrl->SetLimits(cfg.minZoom, cfg.maxZoom, cfg.defaultZoom);
    m_camCtrl->SetZoomFactor(cfg.zoomFactor);
    m_camCtrl->Reset();

    m_draggingBlock = -1;
    m_selectedBlock = -1;

    // Reset touch state
    m_touchDragging = false;
    m_touchPanning = false;
    m_touchDragBlock = -1;
    m_prevPinchDist = 0.0f;
    m_prevTouchCount = 0;

    TraceLog(LOG_INFO, "PlayState: entered with %zu pieces, %zu blocks",
             m_puzzle.pieces.size(), m_blocks.size());
}

// ============================================================
// Exit
// ============================================================
void PlayState::Exit(Game&)
{
    m_blocks.clear();
    m_snap.reset();
    PuzzleGenerator::Unload(m_puzzle);
}

// ============================================================
// Rotate a block 90° CW
// ============================================================
void PlayState::RotateBlock(int blockId)
{
    for (auto& b : m_blocks)
    {
        if (b.id != blockId) continue;

        b.rotation = (b.rotation + 1) % 4;

        for (int pid : b.pieceIds)
        {
            auto& p = m_puzzle.pieces[pid];
            float dx = p.position.x - b.position.x;
            float dy = p.position.y - b.position.y;
            p.position.x = b.position.x - dy;
            p.position.y = b.position.y + dx;
            p.rotation = (p.rotation + 1) % 4;
        }

        float minX = FLT_MAX, minY = FLT_MAX, maxX = -FLT_MAX, maxY = -FLT_MAX;
        for (int pid : b.pieceIds)
        {
            const auto& p = m_puzzle.pieces[pid];
            float hw = (float)p.texture.width / 2.0f;
            float hh = (float)p.texture.height / 2.0f;
            if (p.position.x - hw < minX) minX = p.position.x - hw;
            if (p.position.y - hh < minY) minY = p.position.y - hh;
            if (p.position.x + hw > maxX) maxX = p.position.x + hw;
            if (p.position.y + hh > maxY) maxY = p.position.y + hh;
        }
        b.bounds.x = minX; b.bounds.y = minY;
        b.bounds.width = maxX - minX; b.bounds.height = maxY - minY;
        b.position.x = minX + (maxX - minX) / 2.0f;
        b.position.y = minY + (maxY - minY) / 2.0f;

        break;
    }
}

// ============================================================
// Fit all
// ============================================================
void PlayState::FitAll()
{
    if (m_blocks.empty() || !m_camCtrl) return;

    float minX = FLT_MAX, minY = FLT_MAX, maxX = -FLT_MAX, maxY = -FLT_MAX;
    for (const auto& b : m_blocks)
    {
        if (b.bounds.x < minX) minX = b.bounds.x;
        if (b.bounds.y < minY) minY = b.bounds.y;
        if (b.bounds.x + b.bounds.width > maxX) maxX = b.bounds.x + b.bounds.width;
        if (b.bounds.y + b.bounds.height > maxY) maxY = b.bounds.y + b.bounds.height;
    }

    m_camCtrl->FitRectangle(
        Rectangle{ minX, minY, maxX - minX, maxY - minY },
        Vector2{ (float)GetScreenWidth(), (float)GetScreenHeight() },
        100.0f);
}

// ============================================================
// Touch input handling
// ============================================================
void PlayState::HandleTouchInput(Game& game)
{
    int touchCount = GetTouchPointCount();

    // --- Touch just ended ---
    if (m_prevTouchCount > 0 && touchCount == 0)
    {
        if (m_touchDragging && m_wasDragged && m_touchDragBlock >= 0)
        {
            SnapCandidate snap = m_snap->FindSnap(m_puzzle.pieces, m_blocks, m_touchDragBlock);
            if (snap.blockA >= 0 && snap.blockB >= 0)
            {
                m_snap->ApplySnap(m_puzzle.pieces, m_blocks, snap);

                for (size_t i = 0; i < m_blocks.size(); ++i)
                    if (m_blocks[i].id == snap.blockA)
                    {
                        Block blk = m_blocks[i];
                        m_blocks.erase(m_blocks.begin() + i);
                        m_blocks.push_back(blk);
                        break;
                    }

                if (m_blocks.size() == 1)
                {
                    TraceLog(LOG_INFO, "Puzzle completed!");
                    game.ChangeState(std::make_unique<CompletedState>());
                    m_prevTouchCount = 0;
                    return;
                }
            }
        }

        m_touchDragging = false;
        m_touchPanning = false;
        m_touchDragBlock = -1;
        m_draggingBlock = -1;
        m_prevTouchCount = 0;
        return;
    }

    // --- Two-finger pinch ---
    if (touchCount >= 2)
    {
        Vector2 t1 = GetTouchPosition(0);
        Vector2 t2 = GetTouchPosition(1);
        float dist = sqrtf((t2.x - t1.x) * (t2.x - t1.x) +
                           (t2.y - t1.y) * (t2.y - t1.y));

        if (m_prevPinchDist > 0.0f && dist > 0.0f)
        {
            Vector2 centre = { (t1.x + t2.x) / 2.0f, (t1.y + t2.y) / 2.0f };
            float factor = dist / m_prevPinchDist;
            m_camCtrl->ZoomAtPoint(centre, factor);
        }
        m_prevPinchDist = dist;
        m_touchDragging = false;
        m_touchPanning = false;
        m_prevTouchCount = touchCount;
        return;
    }
    m_prevPinchDist = 0.0f;

    // --- Single finger ---
    Vector2 touchPos = GetTouchPosition(0);
    Vector2 touchWorld = GetScreenToWorld2D(touchPos, game.GetCamera());

    // Touch started this frame
    if (m_prevTouchCount == 0 && touchCount > 0)
    {
        // Check HUD buttons first (touch Y < 50)
        if (touchPos.y < 50)
        {
            int sw = GetScreenWidth();
            Rectangle btnMenu   = btnRect((float)(sw - 155), 8.0f, 70.0f, 34.0f);
            Rectangle btnRotate = btnRect((float)(sw - 80), 8.0f, 70.0f, 34.0f);

            if (CheckCollisionPointRec(touchPos, btnRotate) && m_selectedBlock >= 0)
            {
                RotateBlock(m_selectedBlock);
                m_prevTouchCount = touchCount;
                return;
            }
            if (CheckCollisionPointRec(touchPos, btnMenu))
            {
                game.ChangeState(std::make_unique<MenuState>());
                m_prevTouchCount = 0;
                return;
            }
        }

        int hit = PickBlock(touchWorld);
        if (hit >= 0)
        {
            m_touchDragging = true;
            m_touchPanning = false;
            m_touchDragBlock = hit;
            m_selectedBlock = hit;
            m_draggingBlock = hit;
            m_wasDragged = false;

            for (size_t i = 0; i < m_blocks.size(); ++i)
                if (m_blocks[i].id == hit)
                {
                    Block blk = m_blocks[i];
                    m_blocks.erase(m_blocks.begin() + i);
                    m_blocks.push_back(blk);
                    break;
                }

            for (const auto& b : m_blocks)
                if (b.id == hit)
                {
                    m_touchDragOffset.x = touchWorld.x - b.position.x;
                    m_touchDragOffset.y = touchWorld.y - b.position.y;
                    m_dragOffset = m_touchDragOffset;
                    break;
                }
        }
        else
        {
            m_touchPanning = true;
            m_touchDragging = false;
            m_selectedBlock = -1;
        }
    }

    // Drag block
    if (m_touchDragging && touchCount > 0 && m_prevTouchCount > 0)
    {
        Block* block = nullptr;
        for (auto& b : m_blocks)
            if (b.id == m_touchDragBlock) { block = &b; break; }

        if (block)
        {
            float newCX = touchWorld.x - m_touchDragOffset.x;
            float newCY = touchWorld.y - m_touchDragOffset.y;
            float dx = newCX - block->position.x;
            float dy = newCY - block->position.y;

            for (auto& p : m_puzzle.pieces)
                if (p.blockId == m_touchDragBlock)
                { p.position.x += dx; p.position.y += dy; }

            block->position.x = newCX;
            block->position.y = newCY;

            float minX = FLT_MAX, minY = FLT_MAX, maxX = -FLT_MAX, maxY = -FLT_MAX;
            for (int pid : block->pieceIds)
            {
                const auto& p = m_puzzle.pieces[pid];
                float hw = (float)p.texture.width / 2.0f;
                float hh = (float)p.texture.height / 2.0f;
                if (p.position.x - hw < minX) minX = p.position.x - hw;
                if (p.position.y - hh < minY) minY = p.position.y - hh;
                if (p.position.x + hw > maxX) maxX = p.position.x + hw;
                if (p.position.y + hh > maxY) maxY = p.position.y + hh;
            }
            block->bounds.x = minX; block->bounds.y = minY;
            block->bounds.width = maxX - minX; block->bounds.height = maxY - minY;
        }
        m_wasDragged = true;
    }

    // Pan camera
    if (m_touchPanning && touchCount > 0 && m_prevTouchCount > 0)
    {
        Vector2 delta = { 0, 0 };
        if (IsGestureDetected(GESTURE_DRAG))
            delta = GetGestureDragVector();
        m_camCtrl->Pan(delta);
    }

    m_prevTouchCount = touchCount;
}

// ============================================================
// Update
// ============================================================
void PlayState::Update(Game& game)
{
    Camera2D& cam = game.GetCamera();

    Vector2 mouseScreen = GetMousePosition();
    Vector2 mouseWorld = GetScreenToWorld2D(mouseScreen, cam);

    // ---- Touch input (has priority over mouse) ----
    if (GetTouchPointCount() > 0)
    {
        HandleTouchInput(game);
        // Still handle HUD buttons with mouse position (works for touch too on desktop)
        // Keyboard shortcuts and camera wheel also still work
    }

    // ---- Camera: zoom with scroll wheel ----
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f)
        m_camCtrl->ZoomWithScroll(wheel, mouseScreen);

    // ---- Camera: pan with middle mouse ----
    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))
        m_camCtrl->Pan(GetMouseDelta());

    // ---- Keyboard shortcuts ----
    if (IsKeyPressed(KEY_R) && m_selectedBlock >= 0)
        RotateBlock(m_selectedBlock);

    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD))
        m_camCtrl->ZoomIn(mouseScreen);

    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT))
        m_camCtrl->ZoomOut(mouseScreen);

    if (IsKeyPressed(KEY_ZERO))
        m_camCtrl->Reset();

    if (IsKeyPressed(KEY_F))
        FitAll();

    // ---- Mouse block dragging (only if not touch-dragging) ----
    if (GetTouchPointCount() == 0)
    {
        // Start drag
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            // Check HUD buttons
            if (mouseScreen.y < 50)
            {
                int sw = GetScreenWidth();

                Rectangle btnZoomIn  = btnRect(10.0f, 8.0f, 50.0f, 34.0f);
                Rectangle btnZoomOut = btnRect(64.0f, 8.0f, 50.0f, 34.0f);
                Rectangle btnReset   = btnRect(118.0f, 8.0f, 60.0f, 34.0f);
                Rectangle btnFit     = btnRect(182.0f, 8.0f, 60.0f, 34.0f);
                Rectangle btnMenu    = btnRect((float)(sw - 155), 8.0f, 70.0f, 34.0f);
                Rectangle btnRotate  = btnRect((float)(sw - 80), 8.0f, 70.0f, 34.0f);

                if (btnHover(btnZoomIn))
                {
                    m_camCtrl->ZoomIn(mouseScreen);
                    return;
                }
                if (btnHover(btnZoomOut))
                {
                    m_camCtrl->ZoomOut(mouseScreen);
                    return;
                }
                if (btnHover(btnReset))
                {
                    m_camCtrl->Reset();
                    return;
                }
                if (btnHover(btnFit))
                {
                    FitAll();
                    return;
                }
                if (btnHover(btnRotate) && m_selectedBlock >= 0)
                {
                    RotateBlock(m_selectedBlock);
                    return;
                }
                if (btnHover(btnMenu))
                {
                    game.ChangeState(std::make_unique<MenuState>());
                    return;
                }
            }

            // Not on HUD — try to pick a block
            int hit = PickBlock(mouseWorld);
            if (hit >= 0)
            {
                m_draggingBlock = hit;
                m_selectedBlock = hit;
                m_wasDragged = false;

                for (size_t i = 0; i < m_blocks.size(); ++i)
                    if (m_blocks[i].id == hit)
                    {
                        Block blk = m_blocks[i];
                        m_blocks.erase(m_blocks.begin() + i);
                        m_blocks.push_back(blk);
                        break;
                    }

                for (const auto& b : m_blocks)
                    if (b.id == hit)
                    {
                        m_dragOffset.x = mouseWorld.x - b.position.x;
                        m_dragOffset.y = mouseWorld.y - b.position.y;
                        break;
                    }
            }
            else
            {
                m_selectedBlock = -1;
            }
        }

        // Drag
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && m_draggingBlock >= 0)
        {
            Block* draggedBlock = nullptr;
            for (auto& b : m_blocks)
                if (b.id == m_draggingBlock) { draggedBlock = &b; break; }

            if (draggedBlock)
            {
                float newCX = mouseWorld.x - m_dragOffset.x;
                float newCY = mouseWorld.y - m_dragOffset.y;
                float dx = newCX - draggedBlock->position.x;
                float dy = newCY - draggedBlock->position.y;

                for (auto& p : m_puzzle.pieces)
                    if (p.blockId == m_draggingBlock)
                    { p.position.x += dx; p.position.y += dy; }

                draggedBlock->position.x = newCX;
                draggedBlock->position.y = newCY;

                float minX = FLT_MAX, minY = FLT_MAX, maxX = -FLT_MAX, maxY = -FLT_MAX;
                for (int pid : draggedBlock->pieceIds)
                {
                    const auto& p = m_puzzle.pieces[pid];
                    float hw = (float)p.texture.width / 2.0f;
                    float hh = (float)p.texture.height / 2.0f;
                    if (p.position.x - hw < minX) minX = p.position.x - hw;
                    if (p.position.y - hh < minY) minY = p.position.y - hh;
                    if (p.position.x + hw > maxX) maxX = p.position.x + hw;
                    if (p.position.y + hh > maxY) maxY = p.position.y + hh;
                }
                draggedBlock->bounds.x = minX;
                draggedBlock->bounds.y = minY;
                draggedBlock->bounds.width = maxX - minX;
                draggedBlock->bounds.height = maxY - minY;
            }
            m_wasDragged = true;
        }

        // Release & snap
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        {
            if (m_draggingBlock >= 0 && m_wasDragged)
            {
                SnapCandidate snap = m_snap->FindSnap(m_puzzle.pieces, m_blocks, m_draggingBlock);
                if (snap.blockA >= 0 && snap.blockB >= 0)
                {
                    m_snap->ApplySnap(m_puzzle.pieces, m_blocks, snap);

                    for (size_t i = 0; i < m_blocks.size(); ++i)
                        if (m_blocks[i].id == snap.blockA)
                        {
                            Block blk = m_blocks[i];
                            m_blocks.erase(m_blocks.begin() + i);
                            m_blocks.push_back(blk);
                            break;
                        }

                    if (m_blocks.size() == 1)
                    {
                        TraceLog(LOG_INFO, "Puzzle completed!");
                        game.ChangeState(std::make_unique<CompletedState>());
                        return;
                    }
                }
            }
            m_draggingBlock = -1;
        }
    }
}

// ============================================================
// Draw
// ============================================================
void PlayState::Draw(Game& game)
{
    Camera2D& cam = game.GetCamera();
    ClearBackground(Color{ 25, 25, 25, 255 });

    BeginMode2D(cam);

    float gridW = m_puzzle.cellSize * m_puzzle.cols;
    float gridH = m_puzzle.cellSize * m_puzzle.rows;
    DrawRectangleLines((int)-gridW / 2, (int)-gridH / 2,
                       (int)gridW, (int)gridH, ColorAlpha(WHITE, 0.08f));

    for (const auto& block : m_blocks)
    {
        for (int pid : block.pieceIds)
        {
            const auto& p = m_puzzle.pieces[pid];
            if (p.texture.id == 0) continue;

            float texW = (float)p.texture.width;
            float texH = (float)p.texture.height;

            DrawTexturePro(p.texture,
                Rectangle{ 0, 0, texW, texH },
                Rectangle{ p.position.x - texW / 2.0f,
                           p.position.y - texH / 2.0f, texW, texH },
                Vector2{ 0, 0 },
                (float)(p.rotation * 90),
                WHITE);
        }
    }

    if (m_selectedBlock >= 0)
    {
        for (const auto& b : m_blocks)
            if (b.id == m_selectedBlock)
            {
                DrawRectangleLinesEx(b.bounds, 2.0f, ColorAlpha(SKYBLUE, 0.7f));
                break;
            }
    }

    EndMode2D();

    // ---- HUD ----
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    DrawRectangle(0, 0, sw, 50, ColorAlpha(BLACK, 0.6f));

    drawBtn(btnRect(10.0f, 8.0f, 50.0f, 34.0f), "+", DARKGRAY, true);
    drawBtn(btnRect(64.0f, 8.0f, 50.0f, 34.0f), "-", DARKGRAY, true);
    drawBtn(btnRect(118.0f, 8.0f, 60.0f, 34.0f), "Reset", DARKGRAY, true);
    drawBtn(btnRect(182.0f, 8.0f, 60.0f, 34.0f), "Fit", DARKGRAY, true);
    drawBtn(btnRect((float)(sw - 80), 8.0f, 70.0f, 34.0f), "Rotate",
            m_selectedBlock >= 0 ? DARKGRAY : ColorAlpha(DARKGRAY, 0.3f),
            m_selectedBlock >= 0);
    drawBtn(btnRect((float)(sw - 155), 8.0f, 70.0f, 34.0f), "Menu",
            Color{ 80, 40, 40, 200 }, true);

    int totalPieces = (int)m_puzzle.pieces.size();
    int connected = totalPieces - (int)m_blocks.size();
    float pct = totalPieces > 0 ? (float)connected / totalPieces * 100.0f : 0.0f;
    DrawText(TextFormat("Blocks: %zu / %d  (%.0f%%)",
             m_blocks.size(), totalPieces, pct),
             sw / 2 - 120, sh - 30, 16, LIGHTGRAY);

    DrawText("Scroll: zoom  |  MMB: pan  |  R: rotate  |  F: fit all",
             10, sh - 30, 14, GRAY);
}

// ============================================================
// Pick block under cursor
// ============================================================
int PlayState::PickBlock(Vector2 worldPos) const
{
    for (int i = (int)m_blocks.size() - 1; i >= 0; --i)
    {
        const auto& block = m_blocks[i];
        if (CheckCollisionPointRec(worldPos, block.bounds))
        {
            for (int j = (int)block.pieceIds.size() - 1; j >= 0; --j)
            {
                const auto& p = m_puzzle.pieces[block.pieceIds[j]];
                if (p.texture.id == 0) continue;
                float hw = (float)p.texture.width / 2.0f;
                float hh = (float)p.texture.height / 2.0f;
                if (CheckCollisionPointRec(worldPos,
                        Rectangle{ p.position.x - hw, p.position.y - hh,
                                   (float)p.texture.width, (float)p.texture.height }))
                    return block.id;
            }
        }
    }
    return -1;
}
