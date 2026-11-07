#ifndef POLYGONTRIANGULATOR_H
#define POLYGONTRIANGULATOR_H

#include "raylib.h"
#include <vector>

// Триангуляция простого (в том числе вогнутого) полигона методом отрезания
// ушей. Возвращает тройки индексов вершин исходного контура.
//
// Зачем: рисовать деталь веером треугольников из центроида нельзя — у
// вогнутого контура (пазы) такой веер заходит за его границу и «заполняет»
// часть впадины, из-за чего шип и паз перестают совпадать на вид.
std::vector<int> TriangulatePolygon(const std::vector<Vector2>& polygon);

#endif // POLYGONTRIANGULATOR_H
