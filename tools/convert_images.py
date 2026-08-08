#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Конвертация изображений каталога в сжатый формат (JPEG по умолчанию).

ВНИМАНИЕ: raylib 6.0 (текущая сборка) не читает WebP и JPEG отключён по
умолчанию (SUPPORT_FILEFORMAT_JPG=0). Для JPEG нужно включить флаг при
сборке raylib (см. CMakeLists.txt: add_compile_definitions(SUPPORT_FILEFORMAT_JPG=1)).
WebP в этой версии raylib не поддерживается вовсе.

Использование:
    python tools/convert_images.py                 # PNG/JPG -> JPEG (q85)
    python tools/convert_images.py --format webp   # -> WebP (если raylib поддержит)
    python tools/convert_images.py --quality 90 --max-size 1536
    python tools/convert_images.py --delete-source # удалить исходники после конвертации
    python tools/convert_images.py --dry-run       # только показать, что будет сделано

Структура папок и имена файлов сохраняются (расширение меняется).
После конвертации перегенерируйте каталог: python tools/gen_catalog.py
"""

import argparse
import os
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_IMAGES = os.path.normpath(os.path.join(SCRIPT_DIR, "..", "assets", "images"))

SOURCE_EXTS = (".png", ".jpg", ".jpeg", ".bmp", ".tga", ".gif")


def fmt_size(n):
    return f"{n / (1024 * 1024):.1f} МБ" if n >= 1024 * 1024 else f"{n / 1024:.0f} КБ"


def main():
    ap = argparse.ArgumentParser(description="Convert puzzle images to a compressed format")
    ap.add_argument("dir", nargs="?", default=DEFAULT_IMAGES, help="images dir")
    ap.add_argument("--format", choices=["jpg", "jpeg", "webp"], default="jpg")
    ap.add_argument("--quality", type=int, default=85, help="encoding quality (default 85)")
    ap.add_argument("--max-size", type=int, default=0, help="downscale longer side to this (0 = keep)")
    ap.add_argument("--delete-source", action="store_true", help="remove source files after conversion")
    ap.add_argument("--dry-run", action="store_true", help="only list what would be converted")
    args = ap.parse_args()

    if not os.path.isdir(args.dir):
        print(f"[ERROR] Directory not found: {args.dir}")
        return 1

    try:
        from PIL import Image
    except ImportError:
        print("[ERROR] Pillow is required:  pip install Pillow")
        return 1

    if args.format == "webp":
        print("[WARN] WebP is NOT supported by raylib 6.0 (this build). "
              "The game will not load converted files until support is added.")
        if not args.dry_run:
            ans = input("Continue anyway? [y/N]: ").strip().lower()
            if ans != "y":
                return 0

    ext_map = {"jpg": "jpg", "jpeg": "jpg", "webp": "webp"}

    files = []
    total_src = 0
    for root, _, names in os.walk(args.dir):
        for name in sorted(names):
            if name.lower().endswith(SOURCE_EXTS):
                p = os.path.join(root, name)
                files.append(p)
                total_src += os.path.getsize(p)

    if not files:
        print("No source images found.")
        return 0

    converted = 0
    total_dst = 0
    for src in files:
        dst = os.path.splitext(src)[0] + "." + ext_map[args.format]
        if os.path.abspath(dst).lower() == os.path.abspath(src).lower():
            continue  # already target format

        size_src = os.path.getsize(src)
        if args.dry_run:
            print(f"  {os.path.relpath(src, args.dir)} -> {os.path.basename(dst)}")
            continue

        try:
            img = Image.open(src)
            img.load()
            if img.mode in ("RGBA", "LA", "P"):
                img = img.convert("RGBA")
                # For JPEG flatten onto white (puzzle images have no alpha)
                if args.format != "webp":
                    bg = Image.new("RGB", img.size, (255, 255, 255))
                    bg.paste(img, mask=img.split()[-1])
                    img = bg
                else:
                    img = img.convert("RGB")
            else:
                img = img.convert("RGB")

            if args.max_size and max(img.size) > args.max_size:
                img.thumbnail((args.max_size, args.max_size), Image.LANCZOS)

            if args.format == "webp":
                img.save(dst, "WEBP", quality=args.quality, method=6)
            else:
                img.save(dst, "JPEG", quality=args.quality, optimize=True,
                         progressive=True)

            size_dst = os.path.getsize(dst)
            total_dst += size_dst
            converted += 1
            print(f"  {os.path.relpath(src, args.dir):60s} "
                  f"{fmt_size(size_src)} -> {fmt_size(size_dst)}")

            if args.delete_source and os.path.abspath(dst) != os.path.abspath(src):
                os.remove(src)
        except Exception as e:
            print(f"  [SKIP] {os.path.relpath(src, args.dir)}: {e}")

    print()
    if args.dry_run:
        print(f"Dry run: {len(files)} files would be converted.")
    else:
        print(f"Converted {converted} files: "
              f"{fmt_size(total_src)} -> {fmt_size(total_dst)} "
              f"({100 * (1 - total_dst / total_src):.0f}% smaller)")
        print("Now regenerate the catalog:  python tools/gen_catalog.py")

    return 0


if __name__ == "__main__":
    sys.exit(main())
