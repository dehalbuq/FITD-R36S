#!/usr/bin/env python3
"""Safe, idempotent Lakka FPS override repair for cached ARMHF source trees.

A previous combination of v2 and Lakka patches could leave two contiguous
r36sFps declarations in bgfxGlue.cpp; Clang then fails with redefinition.
This repair removes only a verified duplicate override, never gameplay code.
"""
from pathlib import Path
import re
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: patch_lakka_fps.py SOURCE_DIR")

p = Path(sys.argv[1]) / "FitdLib/bgfxGlue.cpp"
s = p.read_text()

fps_block = re.compile(
    r'(?m)^[ \t]*const char\* r36sFps = SDL_getenv\("FITD_FPS_LIMIT"\);\n'
    r'[ \t]*if \(r36sFps\) \{\n'
    r'[ \t]*int value = SDL_atoi\(r36sFps\);\n'
    r'[ \t]*if \(value [^\n]*\) gFrameLimit = value;\n'
    r'[ \t]*\}\n'
    r'[ \t]*fprintf\(stderr, "\[FITD-R36S\] FPS limit=%d\\n", gFrameLimit\);[^\n]*\n?'
)
matches = list(fps_block.finditer(s))
declarations = s.count('const char* r36sFps = SDL_getenv("FITD_FPS_LIMIT");')

if declarations > 2:
    raise SystemExit(f"Unsafe FPS override: {declarations} declarations; not editing")
if declarations == 2:
    if len(matches) != 2 or s[matches[0].end():matches[1].start()].strip():
        raise SystemExit("Two FPS overrides detected but not in the expected adjacent form; not editing")
    # Prefer the newer/last override: it may support a wider range or uncapped mode.
    s = s[:matches[0].start()] + s[matches[0].end():]
    print("[LAKKA-FPS] removed duplicate override from cached source")

old = "if (value >= 20 && value <= 35) gFrameLimit = value;"
new = "if (value >= 20 && value <= 120) gFrameLimit = value;"
if old in s:
    if s.count(old) != 1:
        raise SystemExit("Multiple old FPS limits remain; not editing")
    s = s.replace(old, new, 1)
    print("[LAKKA-FPS] renderer limit now accepts 20..120 FPS")
elif new in s or "if (value == 0 || (value >= 20 && value <= 120)) gFrameLimit = value;" in s:
    print("[LAKKA-FPS] FPS override already configured")
elif declarations == 0 and 'gFrameLimit = 25;' in s:
    anchor = '    gFrameLimit = 25;'
    if s.count(anchor) != 1:
        raise SystemExit("Unexpected FPS initialization sites; not editing")
    replacement = '''    gFrameLimit = 25;
    const char* r36sFps = SDL_getenv("FITD_FPS_LIMIT");
    if (r36sFps) {
        int value = SDL_atoi(r36sFps);
        if (value >= 20 && value <= 120) gFrameLimit = value;
    }
    fprintf(stderr, "[FITD-R36S] FPS limit=%d\\n", gFrameLimit);'''
    s = s.replace(anchor, replacement, 1)
    print("[LAKKA-FPS] installed missing FPS environment override")
else:
    raise SystemExit("FPS range anchor not found; refusing unsafe modification")

if s.count('const char* r36sFps = SDL_getenv("FITD_FPS_LIMIT");') != 1:
    raise SystemExit("FPS override verification failed")
if s != p.read_text():
    p.write_text(s)
