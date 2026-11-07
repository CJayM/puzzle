#include "puzzle/PolygonTriangulator.h"

#include <cmath>
#include <vector>

namespace {

float Cross(const Vector2& a, const Vector2& b, const Vector2& c)
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

bool PointInTriangle(const Vector2& p, const Vector2& a, const Vector2& b, const Vector2& c)
{
    // Строго внутри: точки на границе (в том числе совпадающие с вершинами —
    // в контуре детали соседние отрезки дают дубли углов) не считаются
    // «содержащимися», иначе ни одно ухо никогда не отрезается
    const float eps = 1e-4f;
    const float d1 = Cross(a, b, p);
    const float d2 = Cross(b, c, p);
    const float d3 = Cross(c, a, p);
    const bool hasNeg = (d1 < -eps) || (d2 < -eps) || (d3 < -eps);
    const bool hasPos = (d1 > eps) || (d2 > eps) || (d3 > eps);
    return !(hasNeg && hasPos);
}

} // namespace

std::vector<int> TriangulatePolygon(const std::vector<Vector2>& polygon)
{
    std::vector<int> tris;
    const int n = (int)polygon.size();
    if (n < 3) return tris;

    // Площадь со знаком: у контура может быть любой обход, поэтому «ухо»
    // проверяем с учётом направления (dir), а сам порядок вершин не меняем
    float area2 = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const Vector2& a = polygon[i];
        const Vector2& b = polygon[(i + 1) % n];
        area2 += a.x * b.y - b.x * a.y;
    }
    const float dir = (area2 >= 0.0f) ? 1.0f : -1.0f;

    // Соседние совпадающие точки (в контуре детали углы дублируются) площади
    // не добавляют, но мешают тесту «уха» — заранее выбрасываем их
    std::vector<int> v;
    v.reserve((size_t)n);
    for (int i = 0; i < n; ++i)
    {
        if (!v.empty())
        {
            const Vector2& prev = polygon[v.back()];
            const Vector2& cur = polygon[i];
            if (fabsf(prev.x - cur.x) < 1e-6f && fabsf(prev.y - cur.y) < 1e-6f)
                continue;
        }
        v.push_back(i);
    }

    tris.reserve((size_t)(v.size() - 2) * 3);

    while ((int)v.size() > 3)
    {
        const int m = (int)v.size();
        bool clipped = false;

        for (int i = 0; i < m && !clipped; ++i)
        {
            const int i0 = v[(i + m - 1) % m];
            const int i1 = v[i];
            const int i2 = v[(i + 1) % m];

            const Vector2& a = polygon[i0];
            const Vector2& b = polygon[i1];
            const Vector2& c = polygon[i2];

            // Вершина должна быть выпуклой для этого направления обхода
            if (Cross(a, b, c) * dir <= 1e-9f) continue;

            // ... и треугольник не должен содержать других вершин контура
            bool contains = false;
            for (int k = 0; k < m && !contains; ++k)
            {
                const int ik = v[k];
                if (ik == i0 || ik == i1 || ik == i2) continue;
                if (PointInTriangle(polygon[ik], a, b, c)) contains = true;
            }
            if (contains) continue;

            tris.push_back(i0);
            tris.push_back(i1);
            tris.push_back(i2);
            v.erase(v.begin() + i);
            clipped = true;
        }

        if (!clipped)
        {
            // Ни одного «уха»: убираем вершину, лежащую на прямой — площадь
            // она не меняет, но из-за неё тест уха может заклинить
            int collinear = -1;
            for (int i = 0; i < m && collinear < 0; ++i)
            {
                const int i0 = v[(i + m - 1) % m];
                const int i1 = v[i];
                const int i2 = v[(i + 1) % m];
                if (fabsf(Cross(polygon[i0], polygon[i1], polygon[i2])) <= 1e-6f)
                    collinear = i;
            }
            if (collinear >= 0)
            {
                v.erase(v.begin() + collinear);
                continue;
            }

            // Совсем вырожденный контур: добиваем веером, чтобы не зациклиться
            for (int i = 1; i + 1 < (int)v.size(); ++i)
            {
                tris.push_back(v[0]);
                tris.push_back(v[i]);
                tris.push_back(v[i + 1]);
            }
            v.clear();
            break;
        }
    }

    if ((int)v.size() == 3)
    {
        tris.push_back(v[0]);
        tris.push_back(v[1]);
        tris.push_back(v[2]);
    }

    return tris;
}
