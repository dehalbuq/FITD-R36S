#!/usr/bin/env python3
"""Idempotent, narrow SoLoud initialization diagnostics for RK322x ARMHF."""
from pathlib import Path
import sys
root = Path(sys.argv[1])
path = root / "FitdLib/osystemAL.cpp"
src = path.read_text()
marker = "[FITD-LAKKA-AUDIO]"
if marker in src:
    print("SoLoud init diagnostics already applied")
    raise SystemExit(0)
old = """    gSoloud = new SoLoud::Soloud();
    gSoloud->init();"""
new = """    gSoloud = new SoLoud::Soloud();
    // The original code discarded initialization failures, leaving silent gameplay.
    // Keep automatic backend selection until we know which backend is linked.
    const auto audioResult = gSoloud->init();
    std::fprintf(stderr, "[FITD-LAKKA-AUDIO] init result=%d backend=%s samplerate=%u buffer=%u\\n",
        (int)audioResult,
        gSoloud->getBackendString() ? gSoloud->getBackendString() : "(none)",
        gSoloud->getBackendSamplerate(),
        gSoloud->getBackendBufferSize());
    if (audioResult != 0) {
        std::fprintf(stderr, "[FITD-LAKKA-AUDIO] backend initialization failed; audio unavailable\\n");
    }"""
if src.count(old) != 1:
    raise SystemExit("Expected one upstream SoLoud init signature; refusing unsafe patch")
src=src.replace(old,new,1)
if '#include <cstdio>' not in src:
    src='#include <cstdio>\n'+src
path.write_text(src)
print("SoLoud startup diagnostics patched")
