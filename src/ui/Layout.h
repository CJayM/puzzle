#ifndef UI_LAYOUT_H
#define UI_LAYOUT_H

#include "raylib.h"
#include <algorithm>
#include <vector>

// Layout system for positioning UI elements in virtual pixels (dp).
// A Layout takes children (LayoutItem) and assigns them rects according to
// its rule: VLayout stacks vertically, HLayout rows horizontally (with
// stretchable spacers pushing items to the edges), FlowLayout wraps into rows.
// Panels clip their children and provide a scrollbar.
namespace ui {

// Global UI scale helpers (dpiScale * user scale; logical size of the window).
// Defined in ui/Button.cpp.
float UiScale();
float LogW();
float LogH();

// Base class for anything placed inside a Layout (buttons, boxes, spacers)
class LayoutItem
{
public:
    virtual ~LayoutItem() = default;

    // Assign this item's rect (logical dp units)
    virtual void SetRect(float x, float y, float w, float h) = 0;
    // Preferred size in dp
    virtual float MinWidth() const = 0;
    virtual float MinHeight() const = 0;
    // True if the item should absorb leftover space (spacers)
    virtual bool IsStretch() const { return false; }

    // Logical (dp) rect after layout
    virtual float X() const { return 0; }
    virtual float Y() const { return 0; }
    virtual float W() const { return MinWidth(); }
    virtual float H() const { return MinHeight(); }

    // Draw the item (no-op for non-visual items); Panel calls this while
    // scissoring to its bounds
    virtual void Draw(Font&) const {}
};

// Fixed-size box: remembers the rect assigned by the layout
class Box : public LayoutItem
{
public:
    Box(float w, float h) : m_w(w), m_h(h) {}

    void SetRect(float x, float y, float w, float h) override
    { m_x = x; m_y = y; m_w = w; m_h = h; }
    float MinWidth() const override { return m_w; }
    float MinHeight() const override { return m_h; }
    float X() const override { return m_x; }
    float Y() const override { return m_y; }

    Rectangle Rect() const { return Rectangle{ m_x, m_y, m_w, m_h }; }

private:
    float m_x = 0, m_y = 0, m_w = 0, m_h = 0;
};

// Empty space between items. Fixed size, or stretchable when created with w <= 0
// (the stretch flag is stored separately so re-layout doesn't lose it).
class Spacer : public LayoutItem
{
public:
    Spacer(float w = 0.0f, float h = 0.0f) : m_w(w), m_h(h), m_stretch(w <= 0.0f) {}

    void SetRect(float, float, float w, float h) override { m_w = w; m_h = h; }
    float MinWidth() const override { return m_stretch ? 0.0f : m_w; }
    float MinHeight() const override { return m_stretch ? 0.0f : m_h; }
    bool IsStretch() const override { return m_stretch; }
    float W() const override { return m_w; }
    float H() const override { return m_h; }

private:
    float m_w, m_h;
    bool m_stretch;
};

// Base container: owns the layout rule, stores children. Also a LayoutItem,
// so layouts can be nested (a parent assigns the rect; SetRect then runs the
// child layout's Compute inside it).
class Layout : public LayoutItem
{
public:
    virtual ~Layout() = default;

    void AddItem(LayoutItem* item) { m_items.push_back(item); }
    void SetSpacing(float gap) { m_gap = gap; }
    const std::vector<LayoutItem*>& Items() const { return m_items; }

    // Assign rects to all children within (x, y, w, h) in dp
    virtual void Compute(float x, float y, float w, float h) = 0;

    // LayoutItem interface: remember the rect and lay children out inside it
    void SetRect(float x, float y, float w, float h) override
    {
        m_x = x; m_y = y; m_w = w; m_h = h;
        Compute(x, y, w, h);
    }
    float MinWidth() const override { return 0.0f; }
    float MinHeight() const override { return 0.0f; }
    float X() const override { return m_x; }
    float Y() const override { return m_y; }
    float W() const override { return m_w; }
    float H() const override { return m_h; }

    // Draw children recursively (labels, buttons, nested layouts)
    void Draw(Font& font) const override
    {
        for (auto* it : m_items)
            it->Draw(font);
    }

    // Total content height after Compute (max child bottom)
    float ContentHeight() const
    {
        float bottom = 0;
        for (auto* it : m_items)
            bottom = std::max(bottom, it->Y() + it->H());
        return bottom;
    }

protected:
    std::vector<LayoutItem*> m_items;
    float m_gap = 0.0f;

private:
    float m_x = 0.0f, m_y = 0.0f, m_w = 0.0f, m_h = 0.0f;
};

// Vertical stack: children placed top-to-bottom, each full width.
// Width 0 = widest child (for auto-sized buttons).
class VLayout : public Layout
{
public:
    float MinWidth() const override
    {
        float w = 0;
        for (auto* it : m_items)
            w = std::max(w, it->MinWidth());
        return w;
    }
    float MinHeight() const override
    {
        float h = m_gap * std::max(0, (int)m_items.size() - 1);
        for (auto* it : m_items)
            h += it->IsStretch() ? 0.0f : it->MinHeight();
        return h;
    }

    void Compute(float x, float y, float w, float h) override
    {
        int n = (int)m_items.size();
        if (n == 0) return;

        if (w <= 0.0f)
        {
            w = 0.0f;
            for (auto* it : m_items)
                w = std::max(w, it->MinWidth());
        }

        float totalGaps = m_gap * std::max(0, n - 1);
        float used = totalGaps;
        int stretches = 0;
        for (auto* it : m_items)
        {
            if (it->IsStretch()) stretches++;
            else used += it->MinHeight();
        }
        float freeH = h - used;
        float per = (stretches > 0 && freeH > 0) ? freeH / stretches : 0.0f;

        float cy = y;
        for (auto* it : m_items)
        {
            float ih = it->IsStretch() ? per : it->MinHeight();
            it->SetRect(x, cy, w, ih);
            cy += ih + m_gap;
        }
    }
};

// Horizontal row: children placed left-to-right; stretchable items (spacers)
// absorb the leftover width. Height 0 = tallest child (auto-sized buttons).
class HLayout : public Layout
{
public:
    float MinWidth() const override
    {
        float w = m_gap * std::max(0, (int)m_items.size() - 1);
        for (auto* it : m_items)
            w += it->IsStretch() ? 0.0f : it->MinWidth();
        return w;
    }
    float MinHeight() const override
    {
        float h = 0;
        for (auto* it : m_items)
            h = std::max(h, it->MinHeight());
        return h;
    }

    void Compute(float x, float y, float w, float h) override
    {
        int n = (int)m_items.size();
        if (n == 0) return;

        if (h <= 0.0f)
        {
            h = 0.0f;
            for (auto* it : m_items)
                h = std::max(h, it->MinHeight());
        }

        float totalGaps = m_gap * std::max(0, n - 1);
        float used = totalGaps;
        int stretches = 0;
        for (auto* it : m_items)
        {
            if (it->IsStretch()) stretches++;
            else used += it->MinWidth();
        }
        float freeW = w - used;
        float per = (stretches > 0 && freeW > 0) ? freeW / stretches : 0.0f;

        float cx = x;
        for (auto* it : m_items)
        {
            float iw = it->IsStretch() ? per : it->MinWidth();
            float ih = it->IsStretch() ? h : it->MinHeight();
            it->SetRect(cx, y, iw, ih);
            cx += iw + m_gap;
        }
    }
};

// Wrapping flow: children placed left-to-right, wrap to a new row when the
// available width is exhausted (used for the thumbnail grid)
class FlowLayout : public Layout
{
public:
    float MinWidth() const override
    {
        float w = 0;
        for (auto* it : m_items)
            w = std::max(w, it->MinWidth());
        return w;
    }
    float MinHeight() const override
    {
        float h = 0;
        for (auto* it : m_items)
            h = std::max(h, it->MinHeight());
        return h;
    }

    void Compute(float x, float y, float w, float h) override
    {
        float cx = x, cy = y;
        float rowH = 0.0f;
        bool firstInRow = true;

        for (auto* it : m_items)
        {
            float iw = it->MinWidth();
            float ih = it->MinHeight();
            if (iw <= 0.0f || ih <= 0.0f) continue;

            // Wrap to the next row if it doesn't fit (always allow the first item)
            if (!firstInRow && cx + iw > x + w)
            {
                cx = x;
                cy += rowH + m_gap;
                rowH = 0.0f;
                firstInRow = true;
            }

            it->SetRect(cx, cy, iw, ih);
            cx += iw + m_gap;
            rowH = std::max(rowH, ih);
            firstInRow = false;
        }
    }
};

// Scrollable container: clips children to its bounds (scissor), supports
// wheel scrolling and a draggable scrollbar. Content is a Layout (e.g.
// FlowLayout for the thumbnail grid); children are drawn via their Draw().
class Panel : public LayoutItem
{
public:
    void SetRect(float x, float y, float w, float h) override
    { m_x = x; m_y = y; m_w = w; m_h = h; }
    float MinWidth() const override { return m_w; }
    float MinHeight() const override { return m_h; }
    float X() const override { return m_x; }
    float Y() const override { return m_y; }

    // Physical (scaled) rectangle — used for hit-testing / scissor
    Rectangle Rect() const
    {
        float s = UiScale();
        return Rectangle{ m_x * s, m_y * s, m_w * s, m_h * s };
    }

    void Attach(Layout* content) { m_content = content; }

    // Re-lay the content with the current scroll and clamp it
    void ComputeChildren()
    {
        if (m_content == nullptr) return;
        m_content->Compute(m_x, m_y - m_scroll, m_w, 1e9f);
        // Content height relative to the panel top, independent of the
        // current scroll offset (ContentHeight() includes the y-origin shift)
        m_contentH = m_content->ContentHeight() - m_y + m_scroll;
        ClampScroll();
    }

    float GetScroll() const { return m_scroll; }
    void SetScroll(float v) { m_scroll = v; ClampScroll(); }
    float MaxScroll() const { return std::max(0.0f, m_contentH - m_h); }

    bool IsHovered() const
    {
        return CheckCollisionPointRec(GetMousePosition(), Rect());
    }

    // True if the given point (physical px) is over the scrollbar thumb
    bool OnThumb(Vector2 p) const
    {
        if (MaxScroll() <= 0.5f) return false;
        float s = UiScale();
        float trackX = (m_x + m_w - 6.0f) * s;
        float trackY = m_y * s;
        float trackH = m_h * s;
        float thumbH = std::max(30.0f * s, trackH * m_h / m_contentH);
        float thumbY = trackY + (trackH - thumbH) * (m_scroll / MaxScroll());
        Rectangle thumb = { trackX, thumbY, 6.0f * s, thumbH };
        return CheckCollisionPointRec(p, thumb);
    }

    // Wheel scrolling + scrollbar dragging. Call once per frame.
    void Update()
    {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f && IsHovered())
            SetScroll(m_scroll - wheel * 60.0f);

        // Scrollbar thumb drag
        if (MaxScroll() > 0.5f)
        {
            float s = UiScale();
            float trackX = m_x + m_w - 6.0f;
            float trackY = m_y;
            float trackH = m_h;
            float thumbH = std::max(30.0f, trackH * m_h / m_contentH);
            float thumbY = trackY + (trackH - thumbH) * (m_scroll / MaxScroll());

            Rectangle thumb = { trackX * s, thumbY * s, 6.0f * s, thumbH * s };
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                CheckCollisionPointRec(GetMousePosition(), thumb))
                m_dragging = true;
            if (m_dragging)
            {
                if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
                    m_dragging = false;
                else
                {
                    float dy = GetMousePosition().y / s - trackY;
                    float ratio = std::clamp(dy / (trackH - thumbH), 0.0f, 1.0f);
                    SetScroll(ratio * MaxScroll());
                }
            }
        }
    }

    // Draw panel background, then children clipped to the panel bounds,
    // then the scrollbar
    void Draw(Font& font) const
    {
        Rectangle r = Rect();

        DrawRectangleRec(r, { 255, 255, 255, 255 });
        DrawRectangleLinesEx(r, 1, ColorAlpha({ 150, 150, 155, 255 }, 0.18f));

        if (m_content != nullptr)
        {
            BeginScissorMode((int)r.x, (int)r.y, (int)r.width, (int)r.height);
            for (auto* it : m_content->Items())
                it->Draw(font);
            EndScissorMode();
        }

        // Scrollbar
        if (MaxScroll() > 0.5f)
        {
            float s = UiScale();
            float trackX = (m_x + m_w - 6.0f) * s;
            float trackY = m_y * s;
            float trackH = m_h * s;
            float thumbH = std::max(30.0f * s, trackH * m_h / m_contentH);
            float thumbY = trackY + (trackH - thumbH) * (m_scroll / MaxScroll());
            DrawRectangle((int)trackX, (int)trackY, (int)(6.0f * s), (int)trackH,
                          ColorAlpha({ 150, 150, 155, 255 }, 0.15f));
            DrawRectangleRounded(
                Rectangle{ trackX, thumbY, 6.0f * s, thumbH }, 3, 1,
                ColorAlpha({ 120, 120, 130, 255 }, 0.6f));
        }
    }

private:
    void ClampScroll()
    {
        m_scroll = std::clamp(m_scroll, 0.0f, MaxScroll());
    }

    float m_x = 0, m_y = 0, m_w = 0, m_h = 0;
    Layout* m_content = nullptr;
    float m_scroll = 0.0f;
    float m_contentH = 0.0f;
    bool m_dragging = false;
};

} // namespace ui

#endif // UI_LAYOUT_H
