#!/usr/bin/env python3
"""Create an embedded, clear CP850 glyph atlas from a freely licensed TTF.
The TTF is used only at BUILD time and is NOT redistributed in the port ZIP.
Default: DejaVu Sans Condensed, installed by the AArch64 CI worker.
The resulting 1bpp glyphs are drawn on the engine's existing 320x200 UI layer.
"""
from pathlib import Path
import math
import os
import sys

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError as error:
    raise SystemExit("Missing Pillow; install python3-pil") from error

if len(sys.argv) != 2:
    raise SystemExit("usage: gen_readable_font.py UPSTREAM_SOURCE_DIR")
root = Path(sys.argv[1])
font_file = Path(os.environ.get(
    "FITD_R36S_BUILD_FONT",
    "/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed.ttf"))
if not font_file.is_file():
    raise SystemExit(f"TTF missing on build host: {font_file}")

glyph_rows = 18
glyph_columns = 16
lines = [
    "#pragma once",
    "#include <cstdint>",
    "// Generated CP850 glyph bitmaps from DejaVu Sans Condensed (Bitstream Vera license).",
    "// Input TTF remains on the build host and is NOT shipped with this game.",
    "struct R36SReadableGlyph { uint16_t rows[18]; uint8_t advance; };",
]

for size in (12, 14):
    font = ImageFont.truetype(str(font_file), size)
    lines.append(f"static const R36SReadableGlyph g_r36sReadable{size}[256] = {{")
    for value in range(256):
        unicode_char = bytes([value]).decode("cp850")
        img = Image.new("L", (glyph_columns, glyph_rows), 0)
        if value >= 32 and unicode_char.isprintable() and unicode_char not in ("\u00a0",):
            draw = ImageDraw.Draw(img)
            # Constant baseline for all characters, including descenders.
            draw.text((0, 13), unicode_char, font=font, fill=255, anchor="ls")
            advance = max(1, min(glyph_columns - 1,
                                  math.ceil(font.getlength(unicode_char))))
        else:
            advance = 4 if value == 32 else 0
        rows = []
        for y in range(glyph_rows):
            row_mask = 0
            for x in range(glyph_columns):
                if img.getpixel((x, y)) >= 98:
                    row_mask |= 0x8000 >> x
            rows.append(row_mask)
        values = ", ".join(f"0x{row_mask:04X}" for row_mask in rows)
        lines.append(f"    {{ {{ {values} }}, {advance} }}, // CP850 {value:02X}")
    lines.append("};")
    print(f"[FITD-FONT] generated DejaVu Sans Condensed {size}px CP850 glyphs")

target = root / "FitdLib" / "r36sReadableFontData.h"
target.write_text("\n".join(lines) + "\n")
print(f"[FITD-FONT] wrote {target} ({target.stat().st_size} bytes)")
