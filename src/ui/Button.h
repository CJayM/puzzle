#ifndef UI_BUTTON_H
#define UI_BUTTON_H

#include "ui/Layout.h"
#include "raylib.h"
#include <string>

// Reusable UI button widget laid out in virtual pixels (dp).
// Sizes/positions are logical units; the widget scales them by the device
// density and the user's UI-size preference (see docs/virtual_pixels.md).
// Button is a LayoutItem, so it can be placed by ui::VLayout/HLayout/etc.
namespace ui {

class Button : public LayoutItem
{
public:
    Button() = default;
    Button(float x, float y, float w, float h, const char* label,
           bool centerText = true);

    // Position/size in logical dp units; w/h == 0 = auto (from label/texture)
    void SetRect(float x, float y, float w, float h) override;
    float MinWidth() const override { return m_w > 0.0f ? m_w : m_autoW; }
    float MinHeight() const override { return m_h > 0.0f ? m_h : m_autoH; }
    float X() const override { return m_x; }
    float Y() const override { return m_y; }
    void SetLabel(const char* label);
    void SetTexture(Texture2D tex); // draw the texture instead of the label
    void SetFill(float fill);       // 0..1 texture opacity fill (progress)
    void SetEnabled(bool enabled);
    void SetCenterText(bool center);
    void SetFontSize(float sizeDp);
    void SetRadius(float radiusDp);

    // Compute and cache the auto size (needs a font). Called by Draw()
    // automatically; call before a Layout::Compute when using auto sizes.
    void UpdateAutoSize(Font& font) const;

    // Colors: (bg, border, text); optional hover/disabled variants
    void SetColors(Color bg, Color border, Color text);
    void SetHoverColors(Color bgHover, Color borderHover, Color textHover);
    void SetDisabledColors(Color bg, Color border, Color text);

    // Physical (scaled) rectangle — used for hit-testing. For auto-sized
    // buttons (w/h == 0) the size is computed from the label in Draw() and
    // cached here.
    Rectangle Rect() const;

    bool Hovered() const;                       // mouse inside
    bool Clicked() const;                       // pressed this frame, hovered, enabled
    bool IsEnabled() const { return m_enabled; }

    void Draw(Font& font) const override;       // virtual (Panel draws children)

private:
    float m_x = 0.0f, m_y = 0.0f, m_w = 0.0f, m_h = 0.0f; // dp; 0 = auto size
    std::string m_label;
    Texture2D m_texture = { 0 }; // optional: draw image instead of label
    float m_fill = 0.0f;         // 0..1 texture progress fill
    bool m_enabled = true;
    bool m_centerText = true;
    float m_fontSize = 18.0f; // dp
    float m_radius = 6.0f;    // dp

    // Auto-size cache (dp), computed from label/texture when w/h are 0
    mutable float m_autoW = 0.0f, m_autoH = 0.0f;
    mutable bool m_autoSizeValid = false;

    Color m_bg          = { 255, 255, 255, 255 };
    Color m_bgHover     = { 235, 240, 250, 255 };
    Color m_border      = { 150, 150, 155, 64 };
    Color m_borderHover = { 55, 110, 210, 255 };
    Color m_text        = { 40, 40, 45, 255 };
    Color m_textHover   = { 40, 40, 45, 255 };

    bool m_useDisabledColors = false;
    Color m_bgDisabled      = { 0, 0, 0, 0 };
    Color m_borderDisabled  = { 0, 0, 0, 0 };
    Color m_textDisabled    = { 0, 0, 0, 0 };
};

} // namespace ui

#endif // UI_BUTTON_H
