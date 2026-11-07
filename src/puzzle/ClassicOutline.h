#ifndef CLASSICOUTLINE_H
#define CLASSICOUTLINE_H

#include "raylib.h"
#include <vector>

// Форма кусочка пазла по piece_maker.py (geesun/piecemaker): у каждого края
// небольшие дуги у углов («плечи»), а шип и паз описаны кубическими Безье.
// Из-за дуг кромки не прямые, поэтому углы детали получаются не
// прямоугольными.
//
// Тип края определяется чётностью (row + col), поэтому любые две соседние
// детали стыкуются без подгонки: шип одной точно входит в паз другой (это
// одна и та же кривая, отражённая относительно кромки).
//
// Координаты — относительно центра ячейки, y вниз. Обход начинается в правом
// нижнем углу и идёт: низ -> лево -> верх -> право.
enum class ClassicEdge
{
    Flat,   // ровный край (граница картинки)
    Tab,    // шип наружу
    Blank   // паз внутрь
};

// Отрезков на одну кривую Безье (на край их четыре)
constexpr int CLASSIC_ARC_SEGMENTS = 28;

// Точек на край: у ровного — один угол, у шипа/паза — четыре кривые
constexpr int ClassicEdgePointCount(ClassicEdge e, int steps = CLASSIC_ARC_SEGMENTS)
{
    return e == ClassicEdge::Flat ? 1 : 4 * (steps + 1);
}

// Ключи шума по краям в порядке: низ, лево, верх, право. Для общего ребра две
// соседние детали передают ОДИН и тот же ключ — поэтому смещение узлов
// совпадает у обеих, и стык остаётся точным.
struct ClassicEdgeKeys
{
    unsigned key[4] = { 0, 0, 0, 0 };
};

// w, h — размер ячейки; steps — отрезков на кривую Безье;
// keys/amplitude — смещение внутренних узлов краёв шумом Перлина
// (amplitude — доля длины края, 0 = без шума)
std::vector<Vector2> BuildClassicOutline(float w, float h,
                                         ClassicEdge bottom, ClassicEdge left,
                                         ClassicEdge top, ClassicEdge right,
                                         int steps,
                                         const ClassicEdgeKeys* keys = nullptr,
                                         float noiseAmplitude = 0.0f);

#endif // CLASSICOUTLINE_H
