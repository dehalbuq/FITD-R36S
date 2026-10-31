#!/usr/bin/env python3
"""Optional clear text for R36S using offline TrueType-rendered CP850 glyphs.

Unlike ReHaunted's high-resolution ImGui text system, this keeps original
320x200 UI/palette, saves, game strings, and background rendering unchanged.
Defaults to a 12px DejaVu Sans Condensed readable font; switch to original:
  FITD_R36S_FONT=classic
Choose larger readable glyphs:
  FITD_R36S_FONT=readable FITD_R36S_FONT_SIZE=14
No runtime TTF loader/library is required; glyphs embedded at build time.
"""
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: patch_readable_font.py UPSTREAM_SOURCE_DIR")
p = Path(sys.argv[1]) / "FitdLib/font.cpp"
s = p.read_text()

def replace_one(label, old, new):
    global s
    n = s.count(old)
    if n != 1:
        raise RuntimeError(f"font.cpp {label}: expected one anchor, got {n}")
    s = s.replace(old, new, 1)

replace_one("include",
'''#include "common.h"''',
'''#include "common.h"
#ifdef FITD_R36S
#include "r36sReadableFontData.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#endif''')

helpers = r'''
#ifdef FITD_R36S
// Generated from DejaVu Sans Condensed at build time (not an imported TTF
// parser or additional runtime dependency). The original bitmap remains
// available without restarting the engine, by selecting it in the launcher.
static bool r36sUseReadableFont()
{
    static const bool enabled = []() {
        const char* mode = std::getenv("FITD_R36S_FONT");
        const bool use = !mode ||
            (std::strcmp(mode, "classic") != 0 &&
             std::strcmp(mode, "original") != 0 &&
             std::strcmp(mode, "0") != 0 &&
             std::strcmp(mode, "off") != 0);
        fprintf(stderr, "[FITD-FONT] mode=%s (FITD_R36S_FONT=classic for original)\n",
                use ? "readable" : "classic");
        return use;
    }();
    return enabled;
}

static const R36SReadableGlyph* r36sFontGlyphs()
{
    static const bool large = []() {
        const char* size = std::getenv("FITD_R36S_FONT_SIZE");
        const bool isLarge = size && std::strcmp(size, "14") == 0;
        fprintf(stderr, "[FITD-FONT] readableSize=%d\n", isLarge ? 14 : 12);
        return isLarge;
    }();
    return large ? g_r36sReadable14 : g_r36sReadable12;
}

static int r36sGlyphAdvance(unsigned char ch)
{
    // Match the engine's inter-word / inter-letter spacing configuration.
    if (ch == 32)
        return g_fontInterWordSpace + g_fontInterLetterSpace + 1;
    if (ch < 32)
        return 0;
    return r36sFontGlyphs()[ch].advance + g_fontInterLetterSpace;
}

static int r36sReadableTextWidth(const u8* text)
{
    if (!text)
        return 0;
    int width = 0;
    while (*text) {
        width += r36sGlyphAdvance(*text);
        ++text;
    }
    return width;
}

static void r36sDrawReadableText(int x, int y, const u8* text)
{
    if (!text)
        return;
    const R36SReadableGlyph* font = r36sFontGlyphs();
    const unsigned char paletteColor = (unsigned char)fontSm3;
    while (*text) {
        const unsigned char ch = *(text++);
        if (ch >= 32 && ch != 32) {
            const R36SReadableGlyph& glyph = font[ch];
            const int columns = glyph.advance > 16 ? 16 : glyph.advance;
            for (int row = 0; row < 18; ++row) {
                const int py = y + row;
                if (py < 0 || py >= 200)
                    continue;
                const uint16_t bits = glyph.rows[row];
                if (!bits)
                    continue;
                for (int col = 0; col < columns; ++col) {
                    const int px = x + col;
                    if (px >= 0 && px < 320 && (bits & (uint16_t)(0x8000u >> col)))
                        uiLayer[py * 320 + px] = paletteColor;
                }
            }
        }
        x += r36sGlyphAdvance(ch);
    }
}
#endif

'''
replace_one("helpers",
'''void SetFont(char* fontData, int color)
{''',
helpers + '''void SetFont(char* fontData, int color)
{''')

replace_one("text measurement",
'''int ExtGetSizeFont(u8* string)
{
    int width = 0;''',
'''int ExtGetSizeFont(u8* string)
{
#ifdef FITD_R36S
    if (r36sUseReadableFont())
        return r36sReadableTextWidth(string);
#endif
    int width = 0;''')

replace_one("glyph rendering",
'''void PrintFont(int x, int y, char* surface, u8* string)
{
    unsigned char character;''',
'''void PrintFont(int x, int y, char* surface, u8* string)
{
#ifdef FITD_R36S
    if (r36sUseReadableFont()) {
        r36sDrawReadableText(x, y, string);
        return;
    }
#endif
    unsigned char character;''')

p.write_text(s)
print("FITD-R36S: optional original/readable CP850 font rendering installed")
