#include "puzzle/ClassicOutline.h"

#include <algorithm>
#include <cmath>
#include <vector>

// ============================================================
// Форма из piece_maker.py: у каждого края небольшие дуги у углов
// («плечи»), а шип и паз описаны кубическими Безье. Из-за дуг кромки не
// прямые, поэтому углы детали получаются не прямоугольными.
//
// Пропорции как в оригинале: arcRatio 0.07, connectRatio 0.30. Единственное
// отличие: там контрольные точки дуги смещены на постоянные 8 px при ячейке
// ~200 px; здесь это доля размера ячейки (SHOULDER_RATIO), иначе на ячейке
// 80 px плечо было бы огромным, а на 1000 px — незаметным.
// ============================================================
namespace {

constexpr float ARC_RATIO      = 0.07f;
constexpr float CONNECT_RATIO  = 0.30f;
constexpr float SHOULDER_RATIO = 0.04f;

// ------------------------------------------------------------
// 1D шум Перлина: плавное случайное смещение узлов вдоль края
// ------------------------------------------------------------
unsigned HashU(unsigned x, unsigned seed)
{
    x ^= seed;
    x *= 0x9E3779B1u;
    x ^= x >> 15;
    x *= 0x85EBCA77u;
    x ^= x >> 13;
    return x;
}

float Gradient1D(int i, unsigned seed)
{
    return (float)(HashU((unsigned)i, seed) & 0xFFFFFFu) / 8388607.5f - 1.0f;
}

float Perlin1D(float t, unsigned seed)
{
    const int i = (int)floorf(t);
    const float f = t - (float)i;
    const float u = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);
    const float g0 = Gradient1D(i, seed);
    const float g1 = Gradient1D(i + 1, seed);
    const float v0 = g0 * f;
    const float v1 = g1 * (f - 1.0f);
    return (v0 + (v1 - v0) * u) * 2.0f;
}

// Три октавы: крупная волна плюс мелкая рябь
float EdgeNoise(float t, unsigned seed)
{
    return 0.55f * Perlin1D(t * 1.5f, seed)
         + 0.30f * Perlin1D(t * 4.0f + 3.7f, seed ^ 0x9E3779B9u)
         + 0.15f * Perlin1D(t * 9.0f + 7.1f, seed ^ 0x85EBCA6Bu);
}

// Смещение контрольных точек кривой делается внутри OutlineGen: шум меняет
// положение (а значит и тангенсы) внутренних узлов Безье, концы кривых
// остаются на месте — иначе разошлись бы швы между краями.


void PushBezier(std::vector<Vector2>& out, const Vector2 (&p)[4], int steps)
{
    for (int i = 0; i <= steps; ++i)
    {
        const float t  = (float)i / (float)steps;
        const float u  = 1.0f - t;
        const float w0 = u * u * u;
        const float w1 = 3.0f * u * u * t;
        const float w2 = 3.0f * u * t * t;
        const float w3 = t * t * t;
        out.push_back({ p[0].x * w0 + p[1].x * w1 + p[2].x * w2 + p[3].x * w3,
                        p[0].y * w0 + p[1].y * w1 + p[2].y * w2 + p[3].y * w3 });
    }
}

// Та же кривая в обратном направлении и/или отражённая по оси — приём из
// piece_maker.py (reversed + смена знака координаты)
void PushBezierMirrored(std::vector<Vector2>& out, const Vector2 (&p)[4],
                        int steps, bool mirrorX)
{
    Vector2 m[4] = { p[3], p[2], p[1], p[0] };
    if (mirrorX)
        for (auto& v : m) v.x = -v.x;
    else
        for (auto& v : m) v.y = -v.y;
    PushBezier(out, m, steps);
}

// Фигура кусочка в координатах с центром в (0,0)
struct OutlineGen
{
    float w, h, halfW, halfH;
    int steps;

    OutlineGen(float w_, float h_, int steps_)
        : w(w_), h(h_), halfW(w_ * 0.5f), halfH(h_ * 0.5f), steps(steps_) {}

    float ArcW()      const { return w * ARC_RATIO; }
    float ArcH()      const { return h * ARC_RATIO; }
    float ConnectW()  const { return w * CONNECT_RATIO; }
    float ConnectH()  const { return h * CONNECT_RATIO; }
    float ShoulderW() const { return w * SHOULDER_RATIO; }
    float ShoulderH() const { return h * SHOULDER_RATIO; }

    // --- шум: уникальность каждой детали ---------------------------------
    unsigned key = 0;       // ключ ребра сетки (общий у двух соседей)
    float noiseAmp = 0.0f;  // доля длины края

    void SetEdgeKey(unsigned k) { key = k; }

    float NoiseAt(float t, int channel) const
    {
        const unsigned s = key ^ (0x9E3779B9u * (unsigned)(channel + 1));
        return std::clamp(EdgeNoise(std::clamp(t, 0.0f, 1.0f), s), -1.0f, 1.0f);
    }

    // Смещение внутренней контрольной точки Безье: по нормали края (меняет
    // форму) и вдоль края (меняет положение и тангенсы). Концы кривых не
    // трогаем — они на углах и на кромке, по ним стыкуются детали.
    Vector2 Jitter(const Vector2& c, float t, bool vertical, float span,
                   int channel) const
    {
        if (noiseAmp <= 0.0f) return c;

        const float amp = noiseAmp * span;
        const float dn = amp * NoiseAt(t, channel * 2);          // по нормали
        const float dtAng = amp * 0.6f * NoiseAt(t, channel * 2 + 1); // вдоль края

        return vertical ? Vector2{ c.x + dn, c.y + dtAng }
                        : Vector2{ c.x + dtAng, c.y + dn };
    }

    // --- правый край: дуга от верхнего или нижнего угла ---
    void RightArc(std::vector<Vector2>& out, bool istop) const
    {
        const Vector2 top    = { halfW, -halfH };
        const Vector2 bottom = { halfW + ArcW(), -ConnectH() * 0.5f };
        const float dw = bottom.x - top.x;
        const float dh = bottom.y - top.y;
        const float span = 2.0f * halfH;

        Vector2 p[4] = {
            top,
            { top.x + dw / 3.0f + ShoulderW(),        top.y + dh / 3.0f },
            { top.x + 2.0f * dw / 3.0f + ShoulderW(), top.y + dh * 2.0f / 3.0f },
            bottom
        };
        p[1] = Jitter(p[1], (p[1].y + halfH) / span, true, span, istop ? 1 : 0);
        p[2] = Jitter(p[2], (p[2].y + halfH) / span, true, span, istop ? 1 : 0);

        if (!istop) PushBezier(out, p, steps);
        else        PushBezierMirrored(out, p, steps, false);
    }

    // --- правый край: перемычка шипа/паза ---
    void RightConnect(std::vector<Vector2>& out, bool left) const
    {
        const Vector2 start = { halfW + ArcW(), -ConnectH() * 0.5f };
        const Vector2 end   = { start.x - ConnectH(), 0.0f };
        const float span = 2.0f * halfH;

        Vector2 p[4] = {
            start,
            { end.x + (start.x - end.x) * 3.0f / 5.0f, -start.y * 2.0f / 3.0f },
            { end.x, 2.0f * start.y },
            end
        };
        p[1] = Jitter(p[1], (p[1].y + halfH) / span, true, span, left ? 3 : 2);
        p[2] = Jitter(p[2], (p[2].y + halfH) / span, true, span, left ? 3 : 2);

        if (!left) PushBezier(out, p, steps);
        else       PushBezierMirrored(out, p, steps, false);
    }

    // --- нижний край: дуга от правого или левого угла ---
    void BottomArc(std::vector<Vector2>& out, bool isLeft) const
    {
        const Vector2 right = { halfW, halfH };
        const Vector2 left  = { ConnectW() * 0.5f, halfH + ArcH() };
        const float dw = right.x - left.x;
        const float dh = left.y - right.y;
        const float span = 2.0f * halfW;

        Vector2 p[4] = {
            right,
            { right.x - dw / 3.0f,        right.y + dh / 3.0f + ShoulderH() },
            { right.x - 2.0f * dw / 3.0f, right.y + dh * 2.0f / 3.0f + ShoulderH() },
            left
        };
        p[1] = Jitter(p[1], (p[1].x + halfW) / span, false, span, isLeft ? 1 : 0);
        p[2] = Jitter(p[2], (p[2].x + halfW) / span, false, span, isLeft ? 1 : 0);

        if (!isLeft) PushBezier(out, p, steps);
        else         PushBezierMirrored(out, p, steps, true);
    }

    // --- нижний край: перемычка ---
    void BottomConnect(std::vector<Vector2>& out, bool left) const
    {
        const Vector2 start = { ConnectW() * 0.5f, halfH + ArcH() };
        const Vector2 end   = { 0.0f, start.y - ConnectW() };
        const float span = 2.0f * halfW;

        Vector2 p[4] = {
            start,
            { -start.x * 2.0f / 3.0f, end.y + (start.y - end.y) * 3.0f / 5.0f },
            { 2.0f * start.x, end.y },
            end
        };
        p[1] = Jitter(p[1], (p[1].x + halfW) / span, false, span, left ? 3 : 2);
        p[2] = Jitter(p[2], (p[2].x + halfW) / span, false, span, left ? 3 : 2);

        if (!left) PushBezier(out, p, steps);
        else       PushBezierMirrored(out, p, steps, true);
    }

    // --- готовые края ---
    void RightBlank(std::vector<Vector2>& out) const
    {
        RightArc(out, false);
        RightConnect(out, false);
        RightConnect(out, true);
        RightArc(out, true);
    }

    void RightTab(std::vector<Vector2>& out) const
    {
        std::vector<Vector2> v;
        RightBlank(v);
        for (auto& p : v) out.push_back({ 2.0f * halfW - p.x, p.y });
    }

    void LeftTab(std::vector<Vector2>& out) const
    {
        std::vector<Vector2> v;
        RightBlank(v);
        for (auto it = v.rbegin(); it != v.rend(); ++it)
            out.push_back({ it->x - w, it->y });
    }

    void LeftBlank(std::vector<Vector2>& out) const
    {
        std::vector<Vector2> v;
        LeftTab(v);
        for (auto& p : v) out.push_back({ -p.x - w, p.y });
    }

    void BottomBlank(std::vector<Vector2>& out) const
    {
        BottomArc(out, false);
        BottomConnect(out, false);
        BottomConnect(out, true);
        BottomArc(out, true);
    }

    void BottomTab(std::vector<Vector2>& out) const
    {
        std::vector<Vector2> v;
        BottomBlank(v);
        for (auto& p : v) out.push_back({ p.x, 2.0f * halfH - p.y });
    }

    void TopTab(std::vector<Vector2>& out) const
    {
        std::vector<Vector2> v;
        BottomBlank(v);
        for (auto it = v.rbegin(); it != v.rend(); ++it)
            out.push_back({ it->x, it->y - h });
    }

    void TopBlank(std::vector<Vector2>& out) const
    {
        std::vector<Vector2> v;
        TopTab(v);
        for (auto& p : v) out.push_back({ p.x, -p.y - h });
    }

    // Ровный край задаётся только конечным углом: предыдущая кривая
    // заканчивается в углу, между углами рисуется прямая
    void PushBottomEdge(std::vector<Vector2>& out, ClassicEdge e) const
    {
        switch (e)
        {
            case ClassicEdge::Flat:  out.push_back({ -halfW, halfH }); break;
            case ClassicEdge::Tab:   BottomTab(out);   break;
            case ClassicEdge::Blank: BottomBlank(out); break;
        }
    }

    void PushLeftEdge(std::vector<Vector2>& out, ClassicEdge e) const
    {
        switch (e)
        {
            case ClassicEdge::Flat:  out.push_back({ -halfW, -halfH }); break;
            case ClassicEdge::Tab:   LeftTab(out);   break;
            case ClassicEdge::Blank: LeftBlank(out); break;
        }
    }

    void PushTopEdge(std::vector<Vector2>& out, ClassicEdge e) const
    {
        switch (e)
        {
            case ClassicEdge::Flat:  out.push_back({ halfW, -halfH }); break;
            case ClassicEdge::Tab:   TopTab(out);   break;
            case ClassicEdge::Blank: TopBlank(out); break;
        }
    }

    void PushRightEdge(std::vector<Vector2>& out, ClassicEdge e) const
    {
        switch (e)
        {
            case ClassicEdge::Flat:  out.push_back({ halfW, halfH }); break;
            case ClassicEdge::Tab:   RightTab(out);   break;
            case ClassicEdge::Blank: RightBlank(out); break;
        }
    }
};

} // namespace

// ============================================================
// Контур детали: обход от правого нижнего угла (низ -> лево -> верх -> право)
// ============================================================
std::vector<Vector2> BuildClassicOutline(float w, float h,
                                         ClassicEdge bottom, ClassicEdge left,
                                         ClassicEdge top, ClassicEdge right,
                                         int steps,
                                         const ClassicEdgeKeys* keys,
                                         float noiseAmplitude)
{
    if (steps < 4) steps = CLASSIC_ARC_SEGMENTS;

    OutlineGen gen(w, h, steps);
    gen.noiseAmp = noiseAmplitude;

    std::vector<Vector2> out;
    out.reserve(4 * 4 * (size_t)(steps + 1) + 8);

    out.push_back({ gen.halfW, gen.halfH }); // правый нижний угол

    std::vector<Vector2> edge;

    edge.clear();
    gen.SetEdgeKey(keys ? keys->key[0] : 0u);
    gen.PushBottomEdge(edge, bottom);
    out.insert(out.end(), edge.begin(), edge.end());

    edge.clear();
    gen.SetEdgeKey(keys ? keys->key[1] : 0u);
    gen.PushLeftEdge(edge, left);
    out.insert(out.end(), edge.begin(), edge.end());

    edge.clear();
    gen.SetEdgeKey(keys ? keys->key[2] : 0u);
    gen.PushTopEdge(edge, top);
    out.insert(out.end(), edge.begin(), edge.end());

    edge.clear();
    gen.SetEdgeKey(keys ? keys->key[3] : 0u);
    gen.PushRightEdge(edge, right);
    out.insert(out.end(), edge.begin(), edge.end());

    return out;
}
