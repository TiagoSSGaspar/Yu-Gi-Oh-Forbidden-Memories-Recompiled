#!/usr/bin/env python3
"""Make the Android launcher icon from one square picture (needs Pillow).

    python3 tools/pc/android/make_icon.py <square picture>

Writes into src/pc/platform/android/res, which package_android.py compiles
into the APK:
  mipmap-<density>/ic_launcher.png             the picture alone (Android 7),
                                               48 dp, corners cut round
  mipmap-<density>/ic_launcher_foreground.png  the adaptive icon's front
                                               layer (Android 8 on), 108 dp
  mipmap-anydpi-v26/ic_launcher.xml            the adaptive icon: that layer
                                               over black
The launcher cuts an adaptive icon to its own shape inside the middle 72 dp,
so the picture fills SIZE_DP of the 108 and only its outer rim falls outside.
"""
import os
import sys

from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
RES = os.path.join(ROOT, "src", "pc", "platform", "android", "res")
DENSITIES = {"mdpi": 1, "hdpi": 1.5, "xhdpi": 2, "xxhdpi": 3, "xxxhdpi": 4}
LEGACY_DP, LAYER_DP, SIZE_DP = 48, 108, 74
RADIUS = 0.196   # the picture's own rounded corners, as a part of its side
ADAPTIVE = """<?xml version="1.0" encoding="utf-8"?>
<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">
    <background android:drawable="@android:color/black" />
    <foreground android:drawable="@mipmap/ic_launcher_foreground" />
</adaptive-icon>
"""


def rounded(picture, side):
    """The picture at side x side pixels, transparent outside its corners."""
    scale = 4   # the mask drawn bigger, then shrunk: smooth edges
    mask = Image.new("L", (side * scale, side * scale), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, side * scale - 1, side * scale - 1),
                                           radius=round(RADIUS * side * scale), fill=255)
    out = picture.resize((side, side), Image.LANCZOS)
    out.putalpha(mask.resize((side, side), Image.LANCZOS))
    return out


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    picture = Image.open(sys.argv[1]).convert("RGB")
    if picture.width != picture.height:
        sys.exit(f"{sys.argv[1]}: {picture.width}x{picture.height}, not square")
    for name, factor in DENSITIES.items():
        folder = os.path.join(RES, f"mipmap-{name}")
        os.makedirs(folder, exist_ok=True)
        rounded(picture, round(LEGACY_DP * factor)).save(os.path.join(folder, "ic_launcher.png"), optimize=True)
        layer = round(LAYER_DP * factor)
        side = round(SIZE_DP * factor)
        canvas = Image.new("RGBA", (layer, layer), (0, 0, 0, 0))
        front = rounded(picture, side)
        canvas.paste(front, ((layer - side) // 2, (layer - side) // 2), front)
        canvas.save(os.path.join(folder, "ic_launcher_foreground.png"), optimize=True)
    folder = os.path.join(RES, "mipmap-anydpi-v26")
    os.makedirs(folder, exist_ok=True)
    with open(os.path.join(folder, "ic_launcher.xml"), "w", encoding="utf-8", newline="\n") as handle:
        handle.write(ADAPTIVE)
    print(f"{os.path.relpath(RES, ROOT)}: launcher icon from {sys.argv[1]}")


if __name__ == "__main__":
    main()
