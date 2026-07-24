#ifndef DRAWHELPER_H
#define DRAWHELPER_H

#include "raylib.h"

// Draw text using the game's custom font (Arial) with a simple interface
// similar to DrawText(). Font size is specified, not dependent on font base size.
inline void DrawGameText(Font& font, const char* text, int posX, int posY,
                          int fontSize, Color color)
{
    if (font.texture.id == 0) return;
    Vector2 p = { (float)posX, (float)posY };
    DrawTextEx(font, text, p, (float)fontSize, 1, color);
}

inline int MeasureGameText(Font& font, const char* text, int fontSize)
{
    if (font.texture.id == 0) return 0;
    Vector2 sz = MeasureTextEx(font, text, (float)fontSize, 1);
    return (int)sz.x;
}

#endif
