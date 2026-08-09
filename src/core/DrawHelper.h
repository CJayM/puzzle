#ifndef DRAWHELPER_H
#define DRAWHELPER_H

#include "raylib.h"

// SDF shader used by DrawGameText (set once after font loading).
// When non-null with a valid id, text is rendered via BeginShaderMode so
// the SDF fragment shader produces sharp glyphs at any scale.
inline Shader* g_fontShader = nullptr;

inline void SetFontShader(Shader* shader) { g_fontShader = shader; }

// Draw text using the game's custom font (Arial) with a simple interface
// similar to DrawText(). Font size is specified, not dependent on font base size.
inline void DrawGameText(Font& font, const char* text, int posX, int posY,
                          int fontSize, Color color)
{
    if (font.texture.id == 0) return;
    Vector2 p = { (float)posX, (float)posY };
    bool sdf = (g_fontShader != nullptr && g_fontShader->id != 0);
    if (sdf) BeginShaderMode(*g_fontShader);
    DrawTextEx(font, text, p, (float)fontSize, 1, color);
    if (sdf) EndShaderMode();
}

inline int MeasureGameText(Font& font, const char* text, int fontSize)
{
    if (font.texture.id == 0) return 0;
    Vector2 sz = MeasureTextEx(font, text, (float)fontSize, 1);
    return (int)sz.x;
}

#endif
