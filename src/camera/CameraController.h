#ifndef CAMERACONTROLLER_H
#define CAMERACONTROLLER_H

#include "raylib.h"

class CameraController
{
public:
    CameraController(Camera2D& camera);

    // Configuration
    void SetLimits(float minZoom, float maxZoom, float defaultZoom);
    void SetZoomFactor(float factor);

    // Zoom relative to a screen-space point (e.g. cursor or pinch centre)
    void ZoomAtPoint(Vector2 screenPoint, float factor);

    // Convenience: zoom in/out at a screen point
    void ZoomIn(Vector2 screenPoint);
    void ZoomOut(Vector2 screenPoint);

    // Zoom with mouse wheel delta (+1 or -1)
    void ZoomWithScroll(float wheelDelta, Vector2 screenPoint);

    // Pan by a screen-space delta (e.g. mouse delta)
    void Pan(Vector2 screenDelta);

    // Reset to default zoom and origin
    void Reset();

    // Fit a world-space rectangle into the viewport
    void FitRectangle(Rectangle worldBounds, Vector2 screenSize, float margin = 100.0f);

    // Getters
    float GetZoom() const;
    float GetMinZoom() const;
    float GetMaxZoom() const;

private:
    Camera2D& m_camera;
    float m_minZoom = 0.2f;
    float m_maxZoom = 4.0f;
    float m_defaultZoom = 1.0f;
    float m_zoomFactor = 1.15f;
};

#endif // CAMERACONTROLLER_H
