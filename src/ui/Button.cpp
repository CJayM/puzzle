#include "ui/Button.h"
#include "core/Config.h"
#include "states/SettingsState.h"
#include "core/DrawHelper.h"

#include <algorithm>
#include <cstdio>

namespace ui {

float UiScale()
{
    return Config::Instance().dpiScale * SettingsState::UserScale();
}

float LogW() { return GetScreenWidth() / UiScale(); }
float LogH() { return GetScreenHeight() / UiScale(); }

Button::Button(float x, float y, float w, float h, const char* label, bool centerText)
    : m_x(x), m_y(y), m_w(w), m_h(h), m_label(label ? label : ""),
      m_centerText(centerText)
{
}

void Button::SetRect(float x, float y, float w, float h)
{
    m_x = x; m_y = y; m_w = w; m_h = h;
}

void Button::SetLabel(const char* label)
{
    m_label = label ? label : "";
}

void Button::SetTexture(Texture2D tex) { m_texture = tex; }

void Button::SetFill(float fill) { m_fill = fill; }

void Button::SetEnabled(bool enabled) { m_enabled = enabled; }

void Button::SetCenterText(bool center) { m_centerText = center; }

void Button::SetFontSize(float sizeDp) { m_fontSize = sizeDp; }

void Button::SetRadius(float radiusDp) { m_radius = radiusDp; }

void Button::SetColors(Color bg, Color border, Color text)
{
    m_bg = bg;
    m_border = border;
    m_text = text;
}

void Button::SetHoverColors(Color bgHover, Color borderHover, Color textHover)
{
    m_bgHover = bgHover;
    m_borderHover = borderHover;
    m_textHover = textHover;
}

void Button::SetDisabledColors(Color bg, Color border, Color text)
{
    m_bgDisabled = bg;
    m_borderDisabled = border;
    m_textDisabled = text;
    m_useDisabledColors = true;
}

Rectangle Button::Rect() const
{
    float s = UiScale();
    return Rectangle{ m_x * s, m_y * s, m_w * s, m_h * s };
}

bool Button::Hovered() const
{
    return CheckCollisionPointRec(GetMousePosition(), Rect());
}

bool Button::Clicked() const
{
    return m_enabled && Hovered() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

void Button::Draw(Font& font) const
{
    if (m_w <= 0.0f || m_h <= 0.0f) return;

    float s = UiScale();
    Rectangle r = Rect();
    bool hv = m_enabled && Hovered();

    Color bg, border, text;
    if (m_enabled)
    {
        bg = hv ? m_bgHover : m_bg;
        border = hv ? m_borderHover : m_border;
        text = hv ? m_textHover : m_text;
    }
    else if (m_useDisabledColors)
    {
        bg = m_bgDisabled;
        border = m_borderDisabled;
        text = m_textDisabled;
    }
    else
    {
        bg = ColorAlpha(m_bg, 0.3f);
        border = ColorAlpha(m_border, 0.3f);
        text = ColorAlpha(m_text, 0.4f);
    }

    DrawRectangleRounded(r, m_radius * s, 1, bg);
    DrawRectangleRoundedLines(r, m_radius * s, 1, border);

    // Texture mode: draw the image fitted into the button instead of a label.
    // Base is semi-transparent; m_fill makes the bottom part opaque (progress)
    if (m_texture.id != 0 && m_texture.width > 0 && m_texture.height > 0)
    {
        float tw = (float)m_texture.width;
        float th = (float)m_texture.height;
        float sc = std::min(r.width / tw, r.height / th);
        float dw = tw * sc, dh = th * sc;
        float dx = r.x + (r.width - dw) / 2.0f;
        float dy = r.y + (r.height - dh) / 2.0f;

        // Base layer: opaque when complete, semi-transparent otherwise
        float baseAlpha = (m_fill >= 1.0f) ? 1.0f : 0.35f;
        DrawTexturePro(m_texture,
                       Rectangle{ 0, 0, tw, th },
                       Rectangle{ dx, dy, dw, dh },
                       Vector2{ 0, 0 }, 0.0f,
                       ColorAlpha(WHITE, baseAlpha));

        // Progress fill: bottom part becomes fully opaque
        if (m_fill > 0.01f && m_fill < 1.0f)
        {
            float fh = dh * m_fill;
            Rectangle src = { 0, th * (1.0f - m_fill), tw, th * m_fill };
            Rectangle fdst = { dx, dy + dh - fh, dw, fh };
            DrawTexturePro(m_texture, src, fdst, Vector2{ 0, 0 }, 0.0f, WHITE);
        }

        // Percent label at the bottom-right corner
        if (m_fill > 0.01f)
        {
            int fs = (int)(11 * UiScale());
            int pw = (int)(30 * UiScale());
            int ph = (int)(14 * UiScale());
            DrawRectangle((int)(r.x + r.width - pw - 4), (int)(r.y + r.height - ph - 2),
                          pw, ph, ColorAlpha({ 240, 242, 245, 255 }, 0.75f));
            char buf[16];
            snprintf(buf, sizeof(buf), "%d%%", (int)(m_fill * 100.0f));
            DrawGameText(font, buf,
                         (int)(r.x + r.width - pw), (int)(r.y + r.height - ph - 1),
                         fs, { 55, 110, 210, 255 });
        }
        return;
    }

    if (m_label.empty()) return;

    int fs = (int)(m_fontSize * s);
    int tx, ty;
    if (m_centerText)
    {
        tx = (int)r.x + (int)((r.width - MeasureGameText(font, m_label.c_str(), fs)) / 2.0f);
        ty = (int)r.y + (int)((r.height - fs) / 2.0f);
    }
    else
    {
        tx = (int)r.x + (int)(6.0f * s);
        ty = (int)r.y + (int)(6.0f * s);
    }
    DrawGameText(font, m_label.c_str(), tx, ty, fs, text);
}

} // namespace ui
