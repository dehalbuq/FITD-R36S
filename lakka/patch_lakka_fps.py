#!/usr/bin/env python3
"""Incremental Lakka FPS-cap fix for an already patched FITD source tree."""
from pathlib import Path
import sys
if len(sys.argv) != 2:
    raise SystemExit("usage: patch_lakka_fps.py SOURCE_DIR")
p=Path(sys.argv[1]) / "FitdLib/bgfxGlue.cpp"
s=p.read_text()
old="if (value >= 20 && value <= 35) gFrameLimit = value;"
new="if (value >= 20 && value <= 120) gFrameLimit = value;"
if old in s:
    assert s.count(old)==1
    p.write_text(s.replace(old,new,1))
    print("[LAKKA-FPS] renderer limit now accepts 20..120 FPS")
elif new in s:
    print("[LAKKA-FPS] already patched")
elif 'gFrameLimit = 25;' in s:
    # Existing cached source may predate the FPS environment override.
    anchor='    gFrameLimit = 25;'
    assert s.count(anchor)==1, 'unexpected number of FPS initialization sites'
    replacement='''    gFrameLimit = 25;
    const char* r36sFps = SDL_getenv("FITD_FPS_LIMIT");
    if (r36sFps) {
        int value = SDL_atoi(r36sFps);
        if (value >= 20 && value <= 120) gFrameLimit = value;
    }
    fprintf(stderr, "[FITD-R36S] FPS limit=%d\\n", gFrameLimit);'''
    p.write_text(s.replace(anchor,replacement,1))
    print("[LAKKA-FPS] installed missing FPS environment override")
else:
    raise SystemExit("FPS range anchor not found; refusing unsafe modification")
