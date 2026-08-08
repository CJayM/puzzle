#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Генератор каталога картинок пазла (assets/images/catalog.txt).

Сканирует поддиректории assets/images (категории/подкатегории на английском)
и формирует catalog.txt с русскими названиями секций и путями к файлам.

Запуск (из любого места):
    python tools/gen_catalog.py [путь/к/assets/images] [путь/к/catalog.txt]

По умолчанию работает с папкой, соседней со скриптом (../assets/images).
"""

import os
import sys

# ------------------------------------------------------------
# Mapping: English folder name -> Russian section name
# ------------------------------------------------------------
CAT = [
    ("nature_and_landscapes", "Природа и пейзажи"),
    ("animals", "Животные"),
    ("cities_and_architecture", "Города и архитектура"),
    ("art_and_abstraction", "Искусство и абстракция"),
    ("food", "Еда"),
    ("travel_and_maps", "Путешествия и карты"),
    ("kids_themes", "Детские темы"),
    ("transport", "Транспорт"),
    ("collages", "Коллажи"),
    ("seasonal_and_holiday", "Сезонные и праздничные"),
    ("space", "Космос"),
]

SUB = {
    "mountains": "Горы", "lakes": "Озёра", "forests": "Леса", "seas": "Моря",
    "sunsets": "Закаты", "seasonal_scenes": "Сезонные сцены",
    "pets": "Домашние питомцы", "wild_animals": "Дикие звери", "birds": "Птицы",
    "butterflies": "Бабочки", "fish": "Рыбы",
    "panoramas": "Панорамы", "streets": "Улицы", "bridges": "Мосты",
    "castles": "Замки", "landmarks": "Достопримечательности",
    "art_reproductions": "Репродукции картин", "geometric_patterns": "Геометрические узоры",
    "gradients": "Градиенты", "mandalas": "Мандалы",
    "dishes": "Блюда", "fruits": "Фрукты", "sweets": "Сладости", "ingredients": "Ингредиенты",
    "geographic_maps": "Географические карты", "country_views": "Виды стран",
    "cartoon_heroes": "Мультгерои", "characters": "Персонажи", "cars": "Машины",
    "automobiles": "Автомобили", "trains": "Поезда", "planes": "Самолёты", "ships": "Корабли",
    "themed_sets": "Наборы тематических изображений", "retro_postcards": "Ретро-открытки",
    "christmas": "Рождество", "halloween": "Хэллоуин", "easter": "Пасха", "new_year": "Новый год",
    "stars": "Звёзды", "nebulae": "Туманности", "planets": "Планеты", "galaxies": "Галактики",
}

EXTS = (".png", ".jpg", ".jpeg", ".bmp", ".webp")

# If a compressed variant exists, prefer it and hide the lossless original
# (e.g. keep "pic.jpg" and omit "pic.png" from the catalog)
PRIORITY_EXTS = (".jpg", ".jpeg", ".webp")


def img_files(d):
    names = sorted(f for f in os.listdir(d) if os.path.splitext(f)[1].lower() in EXTS)
    best = {}
    for f in names:
        base = os.path.splitext(f)[0].lower()
        ext = os.path.splitext(f)[1].lower()
        if base not in best:
            best[base] = f
        else:
            cur = os.path.splitext(best[base])[1].lower()
            if ext in PRIORITY_EXTS and cur not in PRIORITY_EXTS:
                best[base] = f
    return sorted(best.values())


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    base = os.path.normpath(os.path.join(script_dir, "..", "assets", "images"))
    out_path = os.path.join(base, "catalog.txt")

    # Optional CLI overrides: python gen_catalog.py <images_dir> [catalog.txt]
    if len(sys.argv) >= 2:
        base = os.path.abspath(sys.argv[1])
        out_path = os.path.join(base, "catalog.txt")
    if len(sys.argv) >= 3:
        out_path = os.path.abspath(sys.argv[2])

    if not os.path.isdir(base):
        print(f"[ERROR] Images dir not found: {base}")
        return 1

    lines = [
        "# Каталог картинок пазла",
        "# Формат:",
        "#   [Категория]        — без отступа",
        "#     [Подкатегория]   — с отступом",
        "#       путь/к/файлу   — с отступом (относительно этой папки)",
        "# Строки, начинающиеся с # или ; — комментарии.",
        "",
    ]

    count = 0
    for cat_dir, cat_name in CAT:
        cat_path = os.path.join(base, cat_dir)
        if not os.path.isdir(cat_path):
            continue
        sub_lines = []
        for sub in sorted(os.listdir(cat_path)):
            sub_path = os.path.join(cat_path, sub)
            if not os.path.isdir(sub_path):
                continue
            files = img_files(sub_path)
            if not files:
                continue
            sub_name = SUB.get(sub, sub)
            sub_lines.append(f"  [{sub_name}]")
            for f in files:
                sub_lines.append(f"    {cat_dir}/{sub}/{f}")
                count += 1
        if sub_lines:
            lines.append(f"[{cat_name}]")
            lines.extend(sub_lines)
            lines.append("")

    # Extra folder not present in the category list (e.g. "unsorted")
    extra_name = "unsorted"
    extra_path = os.path.join(base, extra_name)
    if os.path.isdir(extra_path):
        files = img_files(extra_path)
        if files:
            lines.append("[Разное]")
            lines.append("  [Разное]")
            for f in files:
                lines.append(f"    {extra_name}/{f}")
                count += 1
            lines.append("")

    text = "\n".join(lines) + "\n"
    with open(out_path, "w", encoding="utf-8") as fh:
        fh.write(text)

    print(f"OK: {out_path} — {count} images listed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
