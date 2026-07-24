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
    float scatterRadius = m_puzzle.cellSize * std::max(m_puzzle.cols, m_puzzle.rows) * 1.5f;
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

    // Draw a faint grid hint
    float gridW = m_puzzle.cellSize * m_puzzle.cols;
    float gridH = m_puzzle.cellSize * m_puzzle.rows;
    DrawRectangleLines((int)-gridW/2, (int)-gridH/2,
                       (int)gridW, (int)gridH, ColorAlpha(WHITE, 0.08f));

    // Draw all pieces
    for (size_t i = 0; i < m_puzzle.pieces.size(); ++i)
    {
        const auto& p = m_puzzle.pieces[i];
        if (p.texture.id == 0) continue;

        // Use localBounds as the source rect
        // The texture includes margin, so the full texture is drawn
        float texW = (float)p.texture.width;
        float texH = (float)p.texture.height;

        // Destination centred on piece position
        Rectangle dest = {
            p.position.x - texW / 2.0f,
            p.position.y - texH / 2.0f,
            texW, texH
        };

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
    // Iterate in reverse so topmost piece is picked first
    for (int i = (int)m_puzzle.pieces.size() - 1; i >= 0; --i)
    {
        const auto& p = m_puzzle.pieces[i];
        if (p.texture.id == 0) continue;

        float halfW = p.texture.width / 2.0f;
        float halfH = p.texture.height / 2.0f;

        Rectangle bounds = {
            p.position.x - halfW,
            p.position.y - halfH,
            p.texture.width,
            p.texture.height
        };

        if (CheckCollisionPointRec(worldPos, bounds))
            return i;
    }
    return -1;
}
