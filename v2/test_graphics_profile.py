#!/usr/bin/env python3
"""CI preflight for the optional graphics regression baseline."""
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: test_graphics_profile.py UPSTREAM_SOURCE_DIR")
root = Path(sys.argv[1]) / "FitdLib"
render = (root / "rendererBGFX.cpp").read_text()
poly = (root / "renderer.cpp").read_text()
checks = {
    "classic mask state": "bgfx::setState(BGFX_STATE_WRITE_RGB\n            | BGFX_STATE_MSAA" in render,
    "ReHaunted mask opt-in": 'std::strcmp(mode, "rehaunted") == 0' in render,
    "mask log": "[FITD-GFX] MASK_DEPTH=" in render,
    "near-camera option": 'std::strcmp(mode, "on") == 0' in poly,
    "near-camera opt-in": "r36sNearClipEnabled() && g_gameId == AITD1" in poly,
    "near-clip report": "[FITD-GFX] NEAR_PRIMITIVE_REJECT" in poly,
}
for label, ok in checks.items():
    print(f"[FITD-GFX] preflight {label}: {'OK' if ok else 'FAIL'}")
if not all(checks.values()):
    raise SystemExit("Graphics regression A/B patch was not installed correctly")
