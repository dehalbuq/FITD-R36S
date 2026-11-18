#!/usr/bin/env python3
"""Optional, reversible reduction of always-on ImGui desktop UI on RK322x.

The full-screen handheld/TV build has no use for the desktop FPS/volume
menubar, but retaining the UI frame pipeline preserves menus/debug features.
"""
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: patch_lakka_perf.py SOURCE_DIR")
path = Path(sys.argv[1]) / "FitdLib/bgfxGlue.cpp"
s = path.read_text()
old = "    if (ImGui::BeginMainMenuBar())"
new = '    if (!SDL_getenv("FITD_LAKKA_HIDE_IMGUI_BAR") && ImGui::BeginMainMenuBar())'
if new in s:
    print("[LAKKA-PERF] desktop menubar bypass already installed")
elif s.count(old) == 1:
    path.write_text(s.replace(old,new,1))
    print("[LAKKA-PERF] optional desktop menubar bypass installed")
else:
    raise SystemExit("Unexpected ImGui desktop menubar signature; refusing unsafe patch")
