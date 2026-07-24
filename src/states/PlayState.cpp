#include "states/PlayState.h"
#include "core/Game.h"
#include "core/Config.h"
#include "raylib.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

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
                // Recompute bounds from piece positions
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
                b.bounds.x = minX;
                b.bounds.y = minY;
                b.bounds.width = maxX - minX;
                b.bounds.height = maxY - minY;
                break;
            }

    m_draggingBlock = -1;

    TraceLog(LOG_INFO, "PlayState: entered with %zu pieces, %zu blocks",
             m_puzzle.pieces.size(), m_blocks.size());
}

void PlayState::Exit(Game&)
{
    m_blocks.clear();
    m_snap.reset();
    PuzzleGenerator::Unload(m_puzzle);
}

void PlayState::Update(Game& game)
{
    Camera2D& cam = game.GetCamera();
    Vector2 mouseScreen = GetMousePosition();
    Vector2 mouseWorld = GetScreenToWorld2D(mouseScreen, cam);

    // Start drag
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        int hitBlock = PickBlock(mouseWorld);
        if (hitBlock >= 0)
        {
            m_draggingBlock = hitBlock;
            m_wasDragged = false;
            for (const auto& b : m_blocks)
                if (b.id == hitBlock)
                {
                    m_dragOffset.x = mouseWorld.x - b.position.x;
                    m_dragOffset.y = mouseWorld.y - b.position.y;
                    break;
                }
        }
    }

    // Drag
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && m_draggingBlock >= 0)
    {
        for (auto& p : m_puzzle.pieces)
            if (p.blockId == m_draggingBlock)
            {
                p.position.x = mouseWorld.x - m_dragOffset.x;
                p.position.y = mouseWorld.y - m_dragOffset.y;
            }

        for (auto& b : m_blocks)
        {
            if (b.id == m_draggingBlock)
            {
                b.position.x = mouseWorld.x - m_dragOffset.x;
                b.position.y = mouseWorld.y - m_dragOffset.y;

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
                break;
            }
        }
        m_wasDragged = true;
    }

    // Release & snap
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON))
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

void PlayState::Draw(Game& game)
{
    Camera2D& cam = game.GetCamera();
    ClearBackground(Color{ 25, 25, 25, 255 });

    BeginMode2D(cam);

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
                           p.position.y - texH / 2.0f,
                           texW, texH },
                Vector2{ 0, 0 },
                (float)(p.rotation * 90),
                WHITE);
        }
    }

    // Highlight dragged block
    if (m_draggingBlock >= 0)
    {
        for (const auto& b : m_blocks)
        {
            if (b.id == m_draggingBlock)
            {
                DrawRectangleLinesEx(b.bounds, 2.0f, ColorAlpha(SKYBLUE, 0.7f));
                break;
            }
        }
    }

    EndMode2D();

    int connected = (int)(m_puzzle.pieces.size() - m_blocks.size());
    DrawText(TextFormat("Blocks: %zu / %zu  (connected: %d)",
             m_blocks.size(), m_puzzle.pieces.size(), connected),
             10, 40, 16, LIGHTGRAY);
}

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
