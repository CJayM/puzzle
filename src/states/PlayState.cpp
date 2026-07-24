#include "states/PlayState.h"
#include "states/CompletedState.h"
#include "states/MenuState.h"
#include "core/Game.h"
#include "core/Config.h"
#include "core/SaveManager.h"
#include "puzzle/PieceShapeGenerator.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

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

    // Load saved state if continuing
    bool isContinuing = game.continueGame;
    game.continueGame = false;
    if (isContinuing)
    {
        float savedCellSize, savedTime;
        std::vector<Piece> savedPieces;
        if (SaveManager::Load(game.selectedImageIndex, game.selectedRows,
                              game.selectedCols, savedCellSize, savedTime, savedPieces))
        {
            for (auto& sp : savedPieces)
            {
                for (auto& gp : m_puzzle.pieces)
                {
                    if (gp.id == sp.id)
                    {
                        gp.position = sp.position;
                        gp.rotation = sp.rotation;
                        gp.blockId = sp.blockId;
                        break;
                    }
                }
            }
            m_elapsedTime = savedTime;
            TraceLog(LOG_INFO, "PlayState: loaded saved state (%zu pieces)", savedPieces.size());
        }
    }

    float snapThreshold = Config::Instance().snapDistanceFactor * m_puzzle.cellSize;
    m_snap = std::make_unique<SnapSystem>(m_puzzle.cellSize, snapThreshold);

    // One block per piece
    // Build blocks from blockIds (groups when continuing, one-per-piece when fresh)
    m_blocks.clear();
    {
        // Collect unique block IDs
        std::vector<int> uniqueBlockIds;
        for (auto& p : m_puzzle.pieces)
        {
            bool found = false;
            for (int bid : uniqueBlockIds)
                if (bid == p.blockId) { found = true; break; }
            if (!found) uniqueBlockIds.push_back(p.blockId);
        }

        m_blocks.reserve(uniqueBlockIds.size());
        for (int bid : uniqueBlockIds)
        {
            Block block;
            block.id = bid;
            block.rotation = 0;

            // Collect all pieces with this blockId
            float minX = FLT_MAX, minY = FLT_MAX, maxX = -FLT_MAX, maxY = -FLT_MAX;
            for (auto& p : m_puzzle.pieces)
            {
                if (p.blockId == bid)
                {
                    block.pieceIds.push_back(p.id);
                    float hw = m_puzzle.cellSize * 0.75f;
                    float hh = m_puzzle.cellSize * 0.75f;
                    if (p.position.x - hw < minX) minX = p.position.x - hw;
                    if (p.position.y - hh < minY) minY = p.position.y - hh;
                    if (p.position.x + hw > maxX) maxX = p.position.x + hw;
                    if (p.position.y + hh > maxY) maxY = p.position.y + hh;
                }
            }
            block.bounds.x = minX; block.bounds.y = minY;
            block.bounds.width = maxX - minX; block.bounds.height = maxY - minY;
            block.position.x = minX + (maxX - minX) / 2.0f;
            block.position.y = minY + (maxY - minY) / 2.0f;

            m_blocks.push_back(block);
        }
    }

    // Scatter only if starting fresh (not continuing)
    if (!isContinuing)
    {
        float scatterRadius = m_puzzle.cellSize * (float)std::max(m_puzzle.cols, m_puzzle.rows) * 1.5f;
        PuzzleGenerator::ScatterPieces(m_puzzle.pieces, scatterRadius);

        // Update block positions after scatter
        for (auto& p : m_puzzle.pieces)
            for (auto& b : m_blocks)
                if (b.id == p.blockId)
                {
                    b.position = p.position;
                    float minX = FLT_MAX, minY = FLT_MAX, maxX = -FLT_MAX, maxY = -FLT_MAX;
                    for (int pid : b.pieceIds)
                    {
                        const auto& pp = m_puzzle.pieces[pid];
                        float hw = m_puzzle.cellSize * 0.75f;
                        float hh = m_puzzle.cellSize * 0.75f;
                        if (pp.position.x - hw < minX) minX = pp.position.x - hw;
                        if (pp.position.y - hh < minY) minY = pp.position.y - hh;
                        if (pp.position.x + hw > maxX) maxX = pp.position.x + hw;
                        if (pp.position.y + hh > maxY) maxY = pp.position.y + hh;
                    }
                    b.bounds.x = minX; b.bounds.y = minY;
                    b.bounds.width = maxX - minX; b.bounds.height = maxY - minY;
                    break;
                }
    }

    // Reset camera
    auto& cfg = Config::Instance();
    m_camCtrl = std::make_unique<CameraController>(game.GetCamera());
    m_camCtrl->SetLimits(cfg.minZoom, cfg.maxZoom, cfg.defaultZoom);
    m_camCtrl->SetZoomFactor(cfg.zoomFactor);
    m_camCtrl->Reset();

    m_draggingBlock = -1;
    m_selectedBlock = -1;
    m_snapAnim.active = false;
    m_elapsedTime = 0.0f;
    m_wireframe = false;

    // Load click sound from file
    if (!IsAudioDeviceReady())
        TraceLog(LOG_WARNING, "PlayState: audio device not ready");
    else
    {
        m_clickSound = LoadSound("assets/sounds/click.mp3");
        if (m_clickSound.frameCount == 0)
            TraceLog(LOG_WARNING, "PlayState: failed to load click sound");
    }

    // Reset touch state
    m_touchDragging = false;
    m_touchPanning = false;
    m_touchDragBlock = -1;
    m_prevPinchDist = 0.0f;
    m_prevTouchCount = 0;

    TraceLog(LOG_INFO, "PlayState: entered with %zu pieces, %zu blocks",
             m_puzzle.pieces.size(), m_blocks.size());

    // Create shared material from source texture
    m_material = LoadMaterialDefault();
    m_material.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
    m_material.maps[MATERIAL_MAP_DIFFUSE].texture = m_puzzle.sourceTexture;

    // Create meshes for each piece
    CreatePieceMeshes();
}

// ============================================================
// Create a mesh per piece from its polygon outline
// ============================================================
void PlayState::CreatePieceMeshes()
{
    float cs = m_puzzle.cellSize;
    float imgW = m_puzzle.imageWidth;
    float imgH = m_puzzle.imageHeight;
    float offX = m_puzzle.gridOffsetX;
    float offY = m_puzzle.gridOffsetY;
    int margin = (int)(cs * 0.25f);
    PieceShapeGenerator shapeGen(cs, margin);

    m_meshes.resize(m_puzzle.pieces.size(), Mesh{ 0 });

    for (size_t pi = 0; pi < m_puzzle.pieces.size(); ++pi)
    {
        const auto& p = m_puzzle.pieces[pi];
        std::vector<Vector2> poly = shapeGen.BuildOutline(p, cs);
        if (poly.size() < 3) continue;

        // Centroid
        Vector2 cent = { 0, 0 };
        for (auto& v : poly) { cent.x += v.x; cent.y += v.y; }
        cent.x /= (float)poly.size();
        cent.y /= (float)poly.size();

        int triCount = (int)poly.size();
        Mesh mesh = { 0 };
        mesh.vertexCount = triCount * 3;
        mesh.triangleCount = triCount;

        float* verts = (float*)RL_MALLOC(mesh.vertexCount * 3 * sizeof(float));
        float* uvs   = (float*)RL_MALLOC(mesh.vertexCount * 2 * sizeof(float));

        for (int i = 0; i < triCount; ++i)
        {
            int j = (i + 1) % triCount;
            float vxi = poly[i].x, vyi = poly[i].y;
            float vxj = poly[j].x, vyj = poly[j].y;

            // Vertex 0: centroid
            int idx = i * 3;
            float cu = (offX + p.gridX * cs + cent.x) / imgW;
            float cv = (offY + p.gridY * cs + cent.y) / imgH;
            verts[idx * 3 + 0] = cent.x;  verts[idx * 3 + 1] = cent.y;  verts[idx * 3 + 2] = 0;
            uvs[idx * 2 + 0] = cu;  uvs[idx * 2 + 1] = cv;
            idx++;
            float uvi_u = (offX + p.gridX * cs + vxi) / imgW;
            float uvi_v = (offY + p.gridY * cs + vyi) / imgH;
            verts[idx * 3 + 0] = vxi;  verts[idx * 3 + 1] = vyi;  verts[idx * 3 + 2] = 0;
            uvs[idx * 2 + 0] = uvi_u;  uvs[idx * 2 + 1] = uvi_v;
            idx++;
            float uvj_u = (offX + p.gridX * cs + vxj) / imgW;
            float uvj_v = (offY + p.gridY * cs + vyj) / imgH;
            verts[idx * 3 + 0] = vxj;  verts[idx * 3 + 1] = vyj;  verts[idx * 3 + 2] = 0;
            uvs[idx * 2 + 0] = uvj_u;  uvs[idx * 2 + 1] = uvj_v;
        }

        mesh.vertices  = verts;
        mesh.texcoords = uvs;
        UploadMesh(&mesh, false);
        m_meshes[pi] = mesh;
    }

    TraceLog(LOG_INFO, "Created %zu piece meshes", m_meshes.size());
}

// ============================================================
// Exit
// ============================================================
void PlayState::Exit(Game& game)
{
    // Unload meshes
    for (auto& mesh : m_meshes)
        if (mesh.vertexCount > 0)
            UnloadMesh(mesh);
    m_meshes.clear();
    UnloadMaterial(m_material);
    m_material = { 0 };

    // Auto-save puzzle state
    if (!m_puzzle.pieces.empty())
    {
        SaveManager::Save(game.selectedImageIndex, game.selectedRows,
                          game.selectedCols, m_puzzle.cellSize,
                          m_elapsedTime, m_puzzle.pieces);
    }

    if (m_clickSound.frameCount > 0)
        UnloadSound(m_clickSound);
    m_blocks.clear();
    m_snap.reset();
    PuzzleGenerator::Unload(m_puzzle);
}

// ============================================================
// Move a block to a new centre position, updating all pieces + bounds
// ============================================================
void PlayState::MoveBlockTo(int blockId, float newCX, float newCY)
{
    Block* block = nullptr;
    for (auto& b : m_blocks)
        if (b.id == blockId) { block = &b; break; }
    if (!block) return;

    float dx = newCX - block->position.x;
    float dy = newCY - block->position.y;

    for (auto& p : m_puzzle.pieces)
        if (p.blockId == blockId)
        { p.position.x += dx; p.position.y += dy; }

    block->position.x = newCX;
    block->position.y = newCY;

    float minX = FLT_MAX, minY = FLT_MAX, maxX = -FLT_MAX, maxY = -FLT_MAX;
    for (int pid : block->pieceIds)
    {
        const auto& p = m_puzzle.pieces[pid];
        float hw = m_puzzle.cellSize * 0.75f;
        float hh = m_puzzle.cellSize * 0.75f;
        if (p.position.x - hw < minX) minX = p.position.x - hw;
        if (p.position.y - hh < minY) minY = p.position.y - hh;
        if (p.position.x + hw > maxX) maxX = p.position.x + hw;
        if (p.position.y + hh > maxY) maxY = p.position.y + hh;
    }
    block->bounds.x = minX;
    block->bounds.y = minY;
    block->bounds.width = maxX - minX;
    block->bounds.height = maxY - minY;
}

// ============================================================
// Start snap animation (or snap instantly if anim disabled)
// ============================================================
void PlayState::TryStartSnap(int draggedBlockId)
{
    if (draggedBlockId < 0 || !m_snap) return;

    SnapCandidate snap = m_snap->FindSnap(m_puzzle.pieces, m_blocks, draggedBlockId);
    if (snap.blockA < 0 || snap.blockB < 0) return;

    // Play click sound
    if (m_clickSound.frameCount > 0)
        PlaySound(m_clickSound);

    // Compute displacement for the moving block (block B)
    const Piece& pieceA = m_puzzle.pieces[snap.pieceA];
    const Piece& pieceB = m_puzzle.pieces[snap.pieceB];

    // Cell size
    float cellSizeEst = m_puzzle.cellSize;

    int dgx = pieceB.gridX - pieceA.gridX;
    int dgy = pieceB.gridY - pieceA.gridY;

    // Block rotation
    int rot = 0;
    for (const auto& b : m_blocks)
        if (b.id == snap.blockA) { rot = b.rotation; break; }

    // Expected offset (from SnapSystem, but we compute delta here)
    float ox = (float)dgx * cellSizeEst;
    float oy = (float)dgy * cellSizeEst;
    switch (rot % 4)
    {
        case 0: break;
        case 1: { float t = ox; ox = -oy; oy = t; } break;
        case 2: ox = -ox; oy = -oy; break;
        case 3: { float t = ox; ox = oy; oy = -t; } break;
    }

    Vector2 expected = { pieceA.position.x + ox, pieceA.position.y + oy };
    Vector2 delta = { expected.x - pieceB.position.x, expected.y - pieceB.position.y };

    // Store start animation
    m_snapAnim.active = true;
    m_snapAnim.movingBlockId = snap.blockB;
    m_snapAnim.targetBlockId = snap.blockA;
    m_snapAnim.snap = snap;
    m_snapAnim.delta = delta;
    m_snapAnim.elapsed = 0.0f;
    m_snapAnim.duration = 0.15f;
    m_snapAnim.pieceIds.clear();
    m_snapAnim.startPos.clear();

    for (const auto& p : m_puzzle.pieces)
    {
        if (p.blockId == snap.blockB)
        {
            m_snapAnim.pieceIds.push_back(p.id);
            m_snapAnim.startPos.push_back(p.position);
        }
    }
}

// ============================================================
// Complete a snap animation — merge blocks
// ============================================================
void PlayState::CompleteSnap()
{
    if (!m_snapAnim.active) return;

    // Ensure pieces are at final position
    for (size_t i = 0; i < m_snapAnim.pieceIds.size(); ++i)
    {
        int pid = m_snapAnim.pieceIds[i];
        m_puzzle.pieces[pid].position.x = m_snapAnim.startPos[i].x + m_snapAnim.delta.x;
        m_puzzle.pieces[pid].position.y = m_snapAnim.startPos[i].y + m_snapAnim.delta.y;
    }

    // Merge blocks
    m_snap->ApplySnap(m_puzzle.pieces, m_blocks, m_snapAnim.snap);

    // Z-order: merged block to front
    int mergedId = m_snapAnim.snap.blockA;
    for (size_t i = 0; i < m_blocks.size(); ++i)
        if (m_blocks[i].id == mergedId)
        {
            Block blk = m_blocks[i];
            m_blocks.erase(m_blocks.begin() + i);
            m_blocks.push_back(blk);
            break;
        }

    // Check completion is done in Update after CompleteSnap returns
    m_snapAnim.active = false;
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
            float hw = m_puzzle.cellSize * 0.75f;
            float hh = m_puzzle.cellSize * 0.75f;
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
            TryStartSnap(m_touchDragBlock);

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
                SaveManager::Save(game.selectedImageIndex, game.selectedRows,
                                  game.selectedCols, m_puzzle.cellSize,
                                  m_elapsedTime, m_puzzle.pieces);
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
        float newCX = touchWorld.x - m_touchDragOffset.x;
        float newCY = touchWorld.y - m_touchDragOffset.y;
        MoveBlockTo(m_touchDragBlock, newCX, newCY);
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

    // ---- Timer ----
    m_elapsedTime += GetFrameTime();

    // ---- Snap animation update ----
    if (m_snapAnim.active)
    {
        m_snapAnim.elapsed += GetFrameTime();
        if (m_snapAnim.elapsed >= m_snapAnim.duration)
        {
            CompleteSnap();
            // Check completion after merge
            if (m_blocks.size() == 1)
            {
                SaveManager::DeleteSave(game.selectedImageIndex, game.selectedRows,
                                        game.selectedCols);
                game.ChangeState(std::make_unique<CompletedState>());
                return;
            }
        }
        else
        {
            float t = m_snapAnim.elapsed / m_snapAnim.duration;
            // Ease out cubic
            float eased = 1.0f - powf(1.0f - t, 3.0f);
            for (size_t i = 0; i < m_snapAnim.pieceIds.size(); ++i)
            {
                int pid = m_snapAnim.pieceIds[i];
                m_puzzle.pieces[pid].position.x = m_snapAnim.startPos[i].x
                    + m_snapAnim.delta.x * eased;
                m_puzzle.pieces[pid].position.y = m_snapAnim.startPos[i].y
                    + m_snapAnim.delta.y * eased;
            }

            // Don't process other input while animating
            return;
        }
    }

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

    if (IsKeyPressed(KEY_F2))
        m_wireframe = !m_wireframe;

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
                    SaveManager::Save(game.selectedImageIndex, game.selectedRows,
                                      game.selectedCols, m_puzzle.cellSize,
                                      m_elapsedTime, m_puzzle.pieces);
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
            float newCX = mouseWorld.x - m_dragOffset.x;
            float newCY = mouseWorld.y - m_dragOffset.y;
            MoveBlockTo(m_draggingBlock, newCX, newCY);
            m_wasDragged = true;
        }

        // Release & snap
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        {
            if (m_draggingBlock >= 0 && m_wasDragged)
                TryStartSnap(m_draggingBlock);
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

    float cs = m_puzzle.cellSize;

    for (const auto& block : m_blocks)
    {
        for (int pid : block.pieceIds)
        {
            if (pid >= (int)m_meshes.size()) continue;
            const Mesh& mesh = m_meshes[pid];
            if (mesh.vertexCount == 0) continue;

            const auto& p = m_puzzle.pieces[pid];

            // Build transform: T(pos) * R(rot) * T(-center)
            Matrix model = MatrixTranslate(-cs / 2.0f, -cs / 2.0f, 0);
            model = MatrixMultiply(MatrixRotateZ((float)(p.rotation * 90) * DEG2RAD), model);
            model = MatrixMultiply(MatrixTranslate(p.position.x, p.position.y, 0), model);

            // Disable backface culling for 2D mesh rendering
            rlDisableBackfaceCulling();

            // Drop shadow (offset, black tint)
            m_material.maps[MATERIAL_MAP_DIFFUSE].color = ColorAlpha(BLACK, 0.25f);
            Matrix shadowModel = MatrixMultiply(MatrixTranslate(4.0f, 4.0f, 0.0f), model);
            DrawMesh(mesh, m_material, shadowModel);

            // Piece itself
            m_material.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
            DrawMesh(mesh, m_material, model);

            rlEnableBackfaceCulling();

            // Wireframe overlay (F2 to toggle)
            if (m_wireframe)
            {
                rlDisableBackfaceCulling();
                rlDisableColorBlend();
                m_material.maps[MATERIAL_MAP_DIFFUSE].color = ColorAlpha(RED, 0.3f);
                DrawMesh(mesh, m_material, model);
                m_material.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
                rlEnableColorBlend();
                rlEnableBackfaceCulling();
            }
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

    int mins = (int)(m_elapsedTime / 60.0f);
    int secs = (int)(m_elapsedTime) % 60;
    DrawText(TextFormat("Time: %02d:%02d  |  %d/%d pieces  (%.0f%%)",
             mins, secs, connected, totalPieces, pct),
             sw / 2 - 180, sh - 30, 16, LIGHTGRAY);

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
                float hs = m_puzzle.cellSize * 0.75f;
                if (CheckCollisionPointRec(worldPos,
                        Rectangle{ p.position.x - hs, p.position.y - hs,
                                   hs * 2, hs * 2 }))
                    return block.id;
            }
        }
    }
    return -1;
}
