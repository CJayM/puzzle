#include "states/PlayState.h"
#include "core/Game.h"
#include "core/Config.h"
#include "raylib.h"

#include <algorithm>

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

    // Generate puzzle
    PuzzleGenerator gen;
    m_puzzle = gen.Generate(path, game.selectedRows, game.selectedCols);

    if (m_puzzle.pieces.empty())
    {
        TraceLog(LOG_ERROR, "PlayState: no pieces generated");
        return;
    }

    // Scatter pieces
    float scatterRadius = m_puzzle.cellSize * std::max(m_puzzle.cols, m_puzzle.rows);
    PuzzleGenerator::ScatterPieces(m_puzzle.pieces, scatterRadius);

    m_draggingPiece = -1;

    TraceLog(LOG_INFO, "PlayState: entered with %zu pieces", m_puzzle.pieces.size());
}

void PlayState::Exit(Game&)
{
    PuzzleGenerator::Unload(m_puzzle);
}

void PlayState::Update(Game& game)
{
    Camera2D& cam = game.GetCamera();

    // --- Dragging ---
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        Vector2 mouseScreen = GetMousePosition();
        Vector2 mouseWorld = GetScreenToWorld2D(mouseScreen, cam);

        int hit = PickPiece(mouseWorld);
        if (hit >= 0)
        {
            m_draggingPiece = hit;
            // Offset from piece centre to mouse
            auto& p = m_puzzle.pieces[hit];
            m_dragOffset.x = mouseWorld.x - p.position.x;
            m_dragOffset.y = mouseWorld.y - p.position.y;
        }
    }

    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && m_draggingPiece >= 0)
    {
        Vector2 mouseScreen = GetMousePosition();
        Vector2 mouseWorld = GetScreenToWorld2D(mouseScreen, cam);

        auto& p = m_puzzle.pieces[m_draggingPiece];
        p.position.x = mouseWorld.x - m_dragOffset.x;
        p.position.y = mouseWorld.y - m_dragOffset.y;
    }

    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON))
    {
        m_draggingPiece = -1;
    }
}

void PlayState::Draw(Game& game)
{
    Camera2D& cam = game.GetCamera();

    ClearBackground(Color{ 25, 25, 25, 255 });

    BeginMode2D(cam);

    // Draw a faint grid background
    float gridW = m_puzzle.cellSize * m_puzzle.cols;
    float gridH = m_puzzle.cellSize * m_puzzle.rows;
    DrawRectangleLines((int)-gridW/2, (int)-gridH/2,
                       (int)gridW, (int)gridH, ColorAlpha(WHITE, 0.1f));

    // Draw all pieces
    for (size_t i = 0; i < m_puzzle.pieces.size(); ++i)
    {
        const auto& p = m_puzzle.pieces[i];
        if (p.texture.id == 0) continue;

        Rectangle dest = {
            p.position.x,
            p.position.y,
            m_puzzle.cellSize,
            m_puzzle.cellSize
        };

        // Centre the dest rectangle on the piece position
        dest.x -= dest.width / 2.0f;
        dest.y -= dest.height / 2.0f;

        float texW = (float)p.texture.width;
        float texH = (float)p.texture.height;

        DrawTexturePro(p.texture,
            Rectangle{ 0, 0, texW, texH },
            dest,
            Vector2{ 0, 0 },
            (float)(p.rotation * 90),
            WHITE);

        // Highlight the piece being dragged
        if ((int)i == m_draggingPiece)
        {
            DrawRectangleLinesEx(dest, 2.0f, ColorAlpha(SKYBLUE, 0.7f));
        }
    }

    EndMode2D();

    // HUD
    DrawText(TextFormat("Pieces: %zu", m_puzzle.pieces.size()), 10, 40, 16, LIGHTGRAY);
    DrawText(TextFormat("Drag: %s", m_draggingPiece >= 0 ? "YES" : "no"),
             10, 60, 14, GRAY);
}

int PlayState::PickPiece(Vector2 worldPos) const
{
    // Iterate in reverse so topmost (last drawn) piece is picked first
    for (int i = (int)m_puzzle.pieces.size() - 1; i >= 0; --i)
    {
        const auto& p = m_puzzle.pieces[i];
        if (p.texture.id == 0) continue;

        Rectangle bounds = {
            p.position.x - m_puzzle.cellSize / 2.0f,
            p.position.y - m_puzzle.cellSize / 2.0f,
            m_puzzle.cellSize,
            m_puzzle.cellSize
        };

        if (CheckCollisionPointRec(worldPos, bounds))
            return i;
    }
    return -1;
}
