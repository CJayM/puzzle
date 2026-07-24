#include "camera/CameraController.h"
#include <algorithm>
#include <cmath>

CameraController::CameraController(Camera2D& camera)
    : m_camera(camera)
{
}

void CameraController::SetLimits(float minZoom, float maxZoom, float defaultZoom)
{
    m_minZoom = minZoom;
    m_maxZoom = maxZoom;
    m_defaultZoom = defaultZoom;
}

void CameraController::SetZoomFactor(float factor)
{
    m_zoomFactor = factor;
}

void CameraController::ZoomAtPoint(Vector2 screenPoint, float factor)
{
    Vector2 worldBefore = GetScreenToWorld2D(screenPoint, m_camera);
    m_camera.zoom = std::clamp(m_camera.zoom * factor, m_minZoom, m_maxZoom);
    Vector2 worldAfter = GetScreenToWorld2D(screenPoint, m_camera);
    m_camera.target.x += worldBefore.x - worldAfter.x;
    m_camera.target.y += worldBefore.y - worldAfter.y;
}

void CameraController::ZoomIn(Vector2 screenPoint)
{
    ZoomAtPoint(screenPoint, m_zoomFactor);
}

void CameraController::ZoomOut(Vector2 screenPoint)
{
    ZoomAtPoint(screenPoint, 1.0f / m_zoomFactor);
}

void CameraController::ZoomWithScroll(float wheelDelta, Vector2 screenPoint)
{
    if (wheelDelta > 0.0f)
        ZoomIn(screenPoint);
    else if (wheelDelta < 0.0f)
        ZoomOut(screenPoint);
}

void CameraController::Pan(Vector2 screenDelta)
{
    m_camera.target.x -= screenDelta.x / m_camera.zoom;
    m_camera.target.y -= screenDelta.y / m_camera.zoom;
}

void CameraController::Reset()
{
    m_camera.zoom = m_defaultZoom;
    m_camera.target = { 0.0f, 0.0f };
}

void CameraController::FitRectangle(Rectangle worldBounds, Vector2 screenSize, float margin)
{
    float worldW = worldBounds.width + margin * 2.0f;
    float worldH = worldBounds.height + margin * 2.0f;

    if (worldW <= 0.0f || worldH <= 0.0f) return;

    float zoomX = screenSize.x / worldW;
    float zoomY = screenSize.y / worldH;
    m_camera.zoom = std::min(zoomX, zoomY);
    m_camera.zoom = std::clamp(m_camera.zoom, m_minZoom, m_maxZoom);

    m_camera.target.x = worldBounds.x + worldBounds.width / 2.0f;
    m_camera.target.y = worldBounds.y + worldBounds.height / 2.0f;
}

float CameraController::GetZoom() const { return m_camera.zoom; }
float CameraController::GetMinZoom() const { return m_minZoom; }
float CameraController::GetMaxZoom() const { return m_maxZoom; }
