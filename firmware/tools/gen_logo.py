#!/usr/bin/env python3
"""Regenerates main/display/logo_data.h from assets/logo.png.

Previously generated from assets/logo_320x240_preview.png, which has a
solid WHITE background baked into its pixels (not transparent) -- that's
why the boot logo screen showed a white rectangle even after fixing the
initial fillScreen() flash in display.cpp. logo.png itself (1024x512) has
genuine alpha transparency; this composites it onto BLACK instead, matching
the rest of the boot sequence.

Usage: python3 tools/gen_logo.py
"""
import pathlib
from PIL import Image

HERE = pathlib.Path(__file__).resolve().parent
SRC = HERE.parent / "assets" / "logo.png"
DST = HERE.parent / "main" / "display" / "logo_data.h"

WIDTH, HEIGHT = 320, 240


def to_rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def main():
    src = Image.open(SRC).convert("RGBA")

    # Fit within the display, preserving aspect ratio (src is 2:1, the
    # display is 4:3 -- width is the binding constraint), centered on a
    # black canvas.
    scale = min(WIDTH / src.width, HEIGHT / src.height)
    new_w, new_h = round(src.width * scale), round(src.height * scale)
    resized = src.resize((new_w, new_h), Image.LANCZOS)

    canvas = Image.new("RGBA", (WIDTH, HEIGHT), (0, 0, 0, 255))
    off_x, off_y = (WIDTH - new_w) // 2, (HEIGHT - new_h) // 2
    canvas.alpha_composite(resized, (off_x, off_y))

    rgb = canvas.convert("RGB")
    pixels = list(rgb.getdata())
    values = [to_rgb565(r, g, b) for (r, g, b) in pixels]

    lines = []
    lines.append("#pragma once")
    lines.append("// Auto-generated from assets/logo.png by tools/gen_logo.py")
    lines.append("// (InCTF/Amrita boot logo, composited onto black -- see that")
    lines.append("// script's docstring for why this isn't logo_320x240_preview.png)")
    lines.append("// RGB565, row-major, 320x240")
    lines.append("#include <cstdint>")
    lines.append("")
    lines.append(f"constexpr int LOGO_WIDTH = {WIDTH};")
    lines.append(f"constexpr int LOGO_HEIGHT = {HEIGHT};")
    lines.append("")
    lines.append(f"constexpr uint16_t LOGO_BITMAP[LOGO_WIDTH * LOGO_HEIGHT] PROGMEM = {{")
    for i in range(0, len(values), 16):
        chunk = values[i:i + 16]
        lines.append("  " + ", ".join(str(v) for v in chunk) + ",")
    lines.append("};")
    lines.append("")

    DST.write_text("\n".join(lines))
    print(f"Wrote {DST} ({len(values)} pixels)")


if __name__ == "__main__":
    main()
