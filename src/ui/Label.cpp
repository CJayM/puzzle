#include "ui/Label.h"
#include "core/DrawHelper.h"

namespace ui {

Label::Label(const char* text, float fontSize, Color color)
    : m_text(text ? text : ""), m_fontSize(fontSize), m_color(color)
{
}

void Label::SetText(const char* text)
{
    m_text = text ? text : "";
    m_autoSizeValid = false;
}

void Label::SetFontSize(float sizeDp)
{
    m_fontSize = sizeDp;
    m_autoSizeValid = false;
}

void Label::SetColor(Color c) { m_color = c; }

void Label::UpdateAutoSize(Font& font) const
{
    if (m_autoSizeValid) return;
    float s = UiScale();
    int fs = (int)(m_fontSize * s);
    Vector2 ts = MeasureTextEx(font, m_text.c_str(), (float)fs, 1);
    m_autoW = ts.x / s;
    m_autoH = ts.y / s;
    m_autoSizeValid = true;
}

void Label::Draw(Font& font) const
{
    UpdateAutoSize(font);
    if (m_text.empty()) return;
    float s = UiScale();
    int fs = (int)(m_fontSize * s);
    DrawGameText(font, m_text.c_str(), (int)(m_x * s), (int)(m_y * s), fs, m_color);
}

} // namespace ui
