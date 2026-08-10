#ifndef UI_LABEL_H
#define UI_LABEL_H

#include "ui/Layout.h"
#include "raylib.h"
#include <string>

// Text element usable inside layouts. Its size is computed from the text
// (auto-size), so VLayout/HLayout can position it like any other item.
namespace ui {

class Label : public LayoutItem
{
public:
    Label() = default;
    Label(const char* text, float fontSize = 16.0f,
          Color color = Color{ 40, 40, 45, 255 });

    void SetText(const char* text);
    void SetFontSize(float sizeDp);
    void SetColor(Color c);

    void SetRect(float x, float y, float w, float h) override { m_x = x; m_y = y; }
    float MinWidth() const override { return m_autoW; }
    float MinHeight() const override { return m_autoH; }
    float X() const override { return m_x; }
    float Y() const override { return m_y; }
    float W() const override { return m_autoW; }
    float H() const override { return m_autoH; }

    // Compute and cache the size (needs a font); called by Draw() automatically
    void UpdateAutoSize(Font& font) const;
    void Draw(Font& font) const override;

private:
    std::string m_text;
    float m_fontSize = 16.0f; // dp
    Color m_color = { 40, 40, 45, 255 };

    float m_x = 0.0f, m_y = 0.0f;
    mutable float m_autoW = 0.0f, m_autoH = 0.0f;
    mutable bool m_autoSizeValid = false;
};

} // namespace ui

#endif // UI_LABEL_H
