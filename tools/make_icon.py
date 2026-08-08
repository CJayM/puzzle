#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Генерация иконки приложения (пазл 2x2):
  - assets/icon.png      (512x512, для окна приложения)
  - assets/icon.ico      (многоразмерный, для exe-файла Windows)
  - res/mipmap-*/ic_launcher.png  (для Android, при --android-res <dir>)

Использование:
    python tools/make_icon.py
    python tools/make_icon.py --android-res path/to/res
"""

import argparse
import os
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ASSETS = os.path.normpath(os.path.join(SCRIPT_DIR, "..", "assets"))


def draw_icon(size):
    from PIL import Image, ImageDraw
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    pad = int(size * 0.08)
    gap = max(2, int(size * 0.035))
    half = (size - 2 * pad - gap) // 2
    r = max(3, int(half * 0.14))

    # Rounded dark background
    d.rounded_rectangle([0, 0, size - 1, size - 1],
                        radius=int(size * 0.18), fill=(30, 41, 59, 255))

    colors = [(231, 76, 60), (46, 204, 113), (52, 152, 219), (241, 196, 15)]
    cells = [
        (pad, pad),
        (pad + half + gap, pad),
        (pad, pad + half + gap),
        (pad + half + gap, pad + half + gap),
    ]
    for (x, y), c in zip(cells, colors):
        d.rounded_rectangle([x, y, x + half - 1, y + half - 1], radius=r,
                            fill=c + (255,))
    return img


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--android-res", default=None,
                    help="output dir for Android res/mipmap-* (e.g. build/res)")
    args = ap.parse_args()

    try:
        from PIL import Image
    except ImportError:
        print("[ERROR] Pillow is required:  pip install Pillow")
        return 1

    icon_png = os.path.join(ASSETS, "icon.png")
    icon_ico = os.path.join(ASSETS, "icon.ico")

    # PNG (window icon)
    draw_icon(512).save(icon_png, "PNG")
    print(f"OK: {icon_png}")

    # ICO (multi-size, for the exe)
    base = draw_icon(256)
    base.save(icon_ico, "ICO",
              sizes=[(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (16, 16)])
    print(f"OK: {icon_ico}")

    if args.android_res:
        sizes = {
            "mipmap-mdpi": 48,
            "mipmap-hdpi": 72,
            "mipmap-xhdpi": 96,
            "mipmap-xxhdpi": 144,
            "mipmap-xxxhdpi": 192,
        }
        for folder, sz in sizes.items():
            out_dir = os.path.join(args.android_res, folder)
            os.makedirs(out_dir, exist_ok=True)
            draw_icon(sz).save(os.path.join(out_dir, "ic_launcher.png"), "PNG")
        print(f"OK: Android launcher icons in {args.android_res}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
