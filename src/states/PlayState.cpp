#include "states/PlayState.h"
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
    Camera2D& cam = game.GetCamera();
    cam.target = { 0, 0 };
    cam.zoom = Config::Instance().defaultZoom;

    m_draggingBlock = -1;
    m_selectedBlock = -1;

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
void PlayState::RotateBlock(int blockId, Game&)
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

        // Recompute bounds after rotation
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
        b.bounds.x = minX;
        b.bounds.y = minY;
        b.bounds.width = maxX - minX;
        b.bounds.height = maxY - minY;
        b.position.x = minX + (maxX - minX) / 2.0f;
        b.position.y = minY + (maxY - minY) / 2.0f;

        break;
    }
}

// ============================================================
// Fit all: zoom to show all blocks
// ============================================================
void PlayState::FitAll(Game& game)
{
    if (m_blocks.empty()) return;

    Camera2D& cam = game.GetCamera();
    const auto& cfg = Config::Instance();

    // Compute combined bounds of all blocks
    float minX = FLT_MAX, minY = FLT_MAX, maxX = -FLT_MAX, maxY = -FLT_MAX;
    for (const auto& b : m_blocks)
    {
        if (b.bounds.x < minX) minX = b.bounds.x;
        if (b.bounds.y < minY) minY = b.bounds.y;
        if (b.bounds.x + b.bounds.width > maxX) maxX = b.bounds.x + b.bounds.width;
        if (b.bounds.y + b.bounds.height > maxY) maxY = b.bounds.y + b.bounds.height;
    }

    float margin = 100.0f;
    float worldW = (maxX - minX) + margin * 2.0f;
    float worldH = (maxY - minY) + margin * 2.0f;

    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();

    float zoomX = screenW / worldW;
    float zoomY = screenH / worldH;
    cam.zoom = std::min(zoomX, zoomY);
    cam.zoom = std::clamp(cam.zoom, cfg.minZoom, cfg.maxZoom);

    cam.target.x = (minX + maxX) / 2.0f;
    cam.target.y = (minY + maxY) / 2.0f;
}

// ============================================================
// Update
// ============================================================
void PlayState::Update(Game& game)
{
    Camera2D& cam = game.GetCamera();
    const auto& cfg = Config::Instance();

    Vector2 mouseScreen = GetMousePosition();
    Vector2 mouseWorld = GetScreenToWorld2D(mouseScreen, cam);

    // ---- Camera: zoom with scroll wheel ----
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f)
    {
        Vector2 before = GetScreenToWorld2D(mouseScreen, cam);
        float factor = (wheel > 0) ? cfg.zoomFactor : (1.0f / cfg.zoomFactor);
        cam.zoom = std::clamp(cam.zoom * factor, cfg.minZoom, cfg.maxZoom);
        Vector2 after = GetScreenToWorld2D(mouseScreen, cam);
        cam.target.x += before.x - after.x;
        cam.target.y += before.y - after.y;
    }

    // ---- Camera: pan with middle mouse ----
    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))
    {
        Vector2 delta = GetMouseDelta();
        cam.target.x -= delta.x / cam.zoom;
        cam.target.y -= delta.y / cam.zoom;
    }

    // ---- Keyboard shortcuts ----
    if (IsKeyPressed(KEY_R) && m_selectedBlock >= 0)
        RotateBlock(m_selectedBlock, game);

    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD))
    {
        Vector2 before = GetScreenToWorld2D(mouseScreen, cam);
        cam.zoom = std::clamp(cam.zoom * cfg.zoomFactor, cfg.minZoom, cfg.maxZoom);
        Vector2 after = GetScreenToWorld2D(mouseScreen, cam);
        cam.target.x += before.x - after.x;
        cam.target.y += before.y - after.y;
    }
    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT))
    {
        Vector2 before = GetScreenToWorld2D(mouseScreen, cam);
        cam.zoom = std::clamp(cam.zoom / cfg.zoomFactor, cfg.minZoom, cfg.maxZoom);
        Vector2 after = GetScreenToWorld2D(mouseScreen, cam);
        cam.target.x += before.x - after.x;
        cam.target.y += before.y - after.y;
    }
    if (IsKeyPressed(KEY_ZERO))
    {
        cam.zoom = cfg.defaultZoom;
        cam.target = { 0, 0 };
    }
    if (IsKeyPressed(KEY_F))
        FitAll(game);

    // ---- Block dragging ----
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        // Check if pressing a HUD button first (skip drag if so)
        // We'll handle HUD buttons in Draw; for now check the top bar area
        if (mouseScreen.y < 50)
        {
            // HUD area — check buttons
            int sw = GetScreenWidth();

            Rectangle btnZoomIn  = btnRect(10.0f, 8.0f, 50.0f, 34.0f);
            Rectangle btnZoomOut = btnRect(64.0f, 8.0f, 50.0f, 34.0f);
            Rectangle btnReset   = btnRect(118.0f, 8.0f, 60.0f, 34.0f);
            Rectangle btnFit     = btnRect(182.0f, 8.0f, 60.0f, 34.0f);
            Rectangle btnRotate  = btnRect((float)(sw - 80), 8.0f, 70.0f, 34.0f);

            if (btnHover(btnZoomIn))
            {
                Vector2 before = GetScreenToWorld2D(mouseScreen, cam);
                cam.zoom = std::clamp(cam.zoom * cfg.zoomFactor, cfg.minZoom, cfg.maxZoom);
                Vector2 after = GetScreenToWorld2D(mouseScreen, cam);
                cam.target.x += before.x - after.x;
                cam.target.y += before.y - after.y;
                return;
            }
            if (btnHover(btnZoomOut))
            {
                Vector2 before = GetScreenToWorld2D(mouseScreen, cam);
                cam.zoom = std::clamp(cam.zoom / cfg.zoomFactor, cfg.minZoom, cfg.maxZoom);
                Vector2 after = GetScreenToWorld2D(mouseScreen, cam);
                cam.target.x += before.x - after.x;
                cam.target.y += before.y - after.y;
                return;
            }
            if (btnHover(btnReset))
            {
                cam.zoom = cfg.defaultZoom;
                cam.target = { 0, 0 };
                return;
            }
            if (btnHover(btnFit))
            {
                FitAll(game);
                return;
            }
            if (btnHover(btnRotate) && m_selectedBlock >= 0)
            {
                RotateBlock(m_selectedBlock, game);
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
        // Find the dragged block
        Block* draggedBlock = nullptr;
        for (auto& b : m_blocks)
            if (b.id == m_draggingBlock) { draggedBlock = &b; break; }

        if (draggedBlock)
        {
            float newCX = mouseWorld.x - m_dragOffset.x;
            float newCY = mouseWorld.y - m_dragOffset.y;
            float dx = newCX - draggedBlock->position.x;
            float dy = newCY - draggedBlock->position.y;

            // Move each piece by the delta, not to an absolute position
            for (auto& p : m_puzzle.pieces)
                if (p.blockId == m_draggingBlock)
                {
                    p.position.x += dx;
                    p.position.y += dy;
                }

            draggedBlock->position.x = newCX;
            draggedBlock->position.y = newCY;

            // Recompute bounds
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
                m_snap->ApplySnap(m_puzzle.pieces, m_blocks, snap);
        }
        m_draggingBlock = -1;
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

    // Draw combined grid hint
    float gridW = m_puzzle.cellSize * m_puzzle.cols;
    float gridH = m_puzzle.cellSize * m_puzzle.rows;
    DrawRectangleLines((int)-gridW / 2, (int)-gridH / 2,
                       (int)gridW, (int)gridH, ColorAlpha(WHITE, 0.08f));

    // Draw pieces by block
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

    // Highlight selected block
    if (m_selectedBlock >= 0)
    {
        for (const auto& b : m_blocks)
        {
            if (b.id == m_selectedBlock)
            {
                DrawRectangleLinesEx(b.bounds, 2.0f, ColorAlpha(SKYBLUE, 0.7f));
                break;
            }
        }
    }

    // Highlight dragged block
    if (m_draggingBlock >= 0 && m_draggingBlock != m_selectedBlock)
    {
        for (const auto& b : m_blocks)
        {
            if (b.id == m_draggingBlock)
            {
                DrawRectangleLinesEx(b.bounds, 2.0f, ColorAlpha(GREEN, 0.7f));
                break;
            }
        }
    }

    EndMode2D();

    // ---- HUD ----
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    // Top bar background
    DrawRectangle(0, 0, sw, 50, ColorAlpha(BLACK, 0.6f));

    drawBtn(btnRect(10.0f, 8.0f, 50.0f, 34.0f), "+", DARKGRAY, true);
    drawBtn(btnRect(64.0f, 8.0f, 50.0f, 34.0f), "-", DARKGRAY, true);
    drawBtn(btnRect(118.0f, 8.0f, 60.0f, 34.0f), "Reset", DARKGRAY, true);
    drawBtn(btnRect(182.0f, 8.0f, 60.0f, 34.0f), "Fit", DARKGRAY, true);

    // Rotate button (right side)
    drawBtn(btnRect((float)(sw - 80), 8.0f, 70.0f, 34.0f), "Rotate",
            m_selectedBlock >= 0 ? DARKGRAY : ColorAlpha(DARKGRAY, 0.3f),
            m_selectedBlock >= 0);

    // Info text
    int totalPieces = (int)m_puzzle.pieces.size();
    int connected = totalPieces - (int)m_blocks.size();
    float pct = totalPieces > 0 ? (float)connected / totalPieces * 100.0f : 0.0f;
    DrawText(TextFormat("Blocks: %zu / %d  (%.0f%%)",
             m_blocks.size(), totalPieces, pct),
             sw / 2 - 120, sh - 30, 16, LIGHTGRAY);

    // Controls hint
    DrawText("Scroll: zoom  |  MMB: pan  |  R: rotate  |  F: fit all",
             10, sh - 30, 14, GRAY);
}

// ============================================================
// Pick block under cursor (by piece bounding box)
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
