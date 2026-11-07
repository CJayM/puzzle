# SDF-рендеринг текста

Текст интерфейса отрисовывается через **Signed Distance Field (SDF)** — каждый
глиф хранится в текстуре как карта расстояний до контура буквы. Фрагментный
шейдер восстанавливает чёткий контур на лету, поэтому один и тот же аталас
даёт **резкие буквы при любом размере** (от 11 до 50 dp) и любом масштабе UI.

## Зачем

Без SDF растровый (bitmap) шрифт генерируется под конкретный кегль: при
увеличении буквы размываются, при уменьшении — «съедаются» пикселями.
Поскольку интерфейс масштабируется (виртуальные пиксели + настройка размера
UI — см. `docs/virtual_pixels.md`), растровый шрифт давал бы мыльные надписи.
SDF решает это одним аталасом.

## Как устроено

### 1. Загрузка шрифта (SDF-аталас)

В `Game::Init` (`src/core/Game.cpp`) Arial загружается как SDF-шрифт:

```cpp
int fileSize = 0;
unsigned char* fileData = LoadFileData(fontPath, &fileSize);   // arial.ttf

const int baseSize = 16;
m_font.glyphs = LoadFontData(fileData, fileSize, baseSize,
                             cps.data(), (int)cps.size(),
                             FONT_SDF, &m_font.glyphCount);     // <-- FONT_SDF
UnloadFileData(fileData);

Image atlas = GenImageFontAtlas(m_font.glyphs, &m_font.recs,
                                m_font.glyphCount, baseSize, 0, 1); // padding 0, Skyline
m_font.texture = LoadTextureFromImage(atlas);
SetTextureFilter(m_font.texture, TEXTURE_FILTER_BILINEAR);      // обязательно
```

Ключевые моменты:

| Параметр | Значение | Зачем |
|---|---|---|
| `FONT_SDF` | тип загрузки | генерирует карты расстояний вместо растра |
| `baseSize = 16` | базовый кегль аталаса | все размеры текста — масштаб от него |
| `GenImageFontAtlas(..., padding 0, method 1)` | упаковка | Skyline-алгоритм, 0 полей — поля не нужны SDF |
| `TEXTURE_FILTER_BILINEAR` | фильтр текстуры | обязателен: шейдер интерполирует расстояния |

Набор глифов тот же, что и раньше: ASCII (32–126), кириллица (0x400–0x4FF),
спецсимволы UI (тире, стрелки, маркеры).

### 2. SDF-шейдер

Загружается один раз:

```cpp
#if defined(PLATFORM_ANDROID)
    const char* sdfPath = "shaders/glsl100/sdf.fs";      // GLES 2.0
#else
    const char* sdfPath = "assets/shaders/glsl330/sdf.fs"; // OpenGL 3.3
#endif
m_fontShader = LoadShader(0, sdfPath);
SetFontShader(&m_fontShader);
```

Файлы лежат в `assets/shaders/glsl330/sdf.fs` и `assets/shaders/glsl100/sdf.fs`
(скопированы из примеров raylib). Суть фрагментного шейдера:

```glsl
float distanceFromOutline = texture(texture0, fragTexCoord).a - 0.5;
float distanceChangePerFragment = length(vec2(dFdx(distanceFromOutline),
                                              dFdy(distanceFromOutline)));
float alpha = smoothstep(-distanceChangePerFragment,
                          distanceChangePerFragment,
                          distanceFromOutline);
finalColor = vec4(fragColor.rgb, fragColor.a * alpha);
```

- `.a` канала текстуры — **знак расстояния** до контура (0.5 = контур);
- `fwidth` (через dFdx/dFdy) оценивает скорость изменения расстояния на
  экране и даёт **антиалиасинг шириной ~1 пиксель** независимо от масштаба;
- итог — плавная полупрозрачная кромка вместо «лесенки».

### 3. Рисование текста

`DrawGameText` (`src/core/DrawHelper.h`) автоматически включает шейдер:

```cpp
inline Shader* g_fontShader = nullptr;           // выставляется SetFontShader()

inline void DrawGameText(Font& font, const char* text, int posX, int posY,
                         int fontSize, Color color)
{
    if (font.texture.id == 0) return;
    bool sdf = (g_fontShader != nullptr && g_fontShader->id != 0);
    if (sdf) BeginShaderMode(*g_fontShader);
    DrawTextEx(font, text, Vector2{(float)posX, (float)posY},
               (float)fontSize, 1, color);
    if (sdf) EndShaderMode();
}
```

Весь UI (меню, кнопки, плитки, HUD) рисуется через `DrawGameText`, поэтому
SDF применяется везде автоматически. `MeasureGameText` использует
`MeasureTextEx` — работает с SDF-шрифтом без изменений.

## Размеры текста

- Все размеры задаются в **dp** (см. `docs/virtual_pixels.md`); физический
  размер пикселей: `fontSize × UiScale`.
- `baseSize = 16` — аталас 16px; текст 50dp — это масштаб 3.1×. SDF-глифы
  масштабируются без потери резкости (в отличие от растра).
- При **очень малых** размерах (< 10 dp) SDF может давать лёгкие артефакты
  (сглаживание съедает тонкие штрихи) — для таких кеглей при необходимости
  можно поднять `baseSize` до 32 (аталас станет больше).

## Fallback

Если шрифт или шейдер не загрузились, игра не падает:

- `LoadFileData` вернул `nullptr` → `GetFontDefault()` (битмап, без шейдера);
- `LoadFontData` не дал глифов → `GetFontDefault()`;
- `LoadShader` вернул `id == 0` → текст рисуется без `BeginShaderMode`
  (SDF-аталас в этом случае выглядит как «грязная» текстура — на практике
  шейдер загружается, но лог предупреждает: «SDF shader failed to load»).

В `Game::Shutdown` шейдер и шрифт выгружаются (`UnloadShader`, `UnloadFont`),
указатель `g_fontShader` сбрасывается.

## Как добавить новый символ

1. Добавить codepoint в список `cps` в `Game::Init`
   (например, `0x2713` — галочка ✓, `0x25CF` — маркер).
2. Пересобрать — аталас сгенерируется с новым глифом.

Проверить, есть ли глиф в Arial, можно любым просмотрщиком шрифтов Windows:
если символа нет в Arial, он отрисуется как «?» — тогда либо выбрать другой
символ, либо подгрузить дополнительный шрифт.

## Ограничения

- SDF требует `TEXTURE_FILTER_BILINEAR` — отключать нельзя.
- Один SDF-аталас = один шрифт; разные шрифты — отдельные аталасы и шейдер
  один общий (шейдер не зависит от шрифта).
- На Android (GLES 2.0) используется `glsl100`-версия шейдера (`fwidth` в
  ES 2.0 недоступен, там используется своя аппроксимация — файл уже
  адаптирован в raylib).
