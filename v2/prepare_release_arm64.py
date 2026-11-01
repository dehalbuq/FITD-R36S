#!/usr/bin/env python3
"""Disable diagnostics only, after all gameplay and crash-safety patches."""
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: prepare_release_arm64.py SOURCE_DIR")
p = Path(sys.argv[1]) / "FitdLib/r36sCrashDebug.h"
s = p.read_text()
def once(old, new):
    global s
    n = s.count(old)
    if n != 1:
        raise RuntimeError(f"{p}: expected one anchor, found {n}: {old!r}")
    s = s.replace(old, new, 1)
once("#ifdef FITD_R36S\n#include <csignal>", "#if defined(FITD_R36S) && !defined(FITD_R36S_RELEASE)\n#include <csignal>")
once("#else\n#define R36S_TRACE(...) do {} while (0)", "#else\ninline bool r36sDeepTrace() { return false; }\n#define R36S_TRACE(...) do {} while (0)")
p.write_text(s)
print("Release: no signal backtraces or deep trace; all gameplay fixes retained")

# Correct the old frame-driven minimum tick. At 60 FPS a forced tick on
# every frame advances the 25Hz world clock 2.4x too quickly.
p = Path(sys.argv[1]) / "FitdLib/osystemSDL.cpp"
s = p.read_text()
old = """    u32 numFramesToAdvance = (SDL_GetTicks() - lastFrameTime) / (1000 / FRAMES_PER_SECOND);

    if (numFramesToAdvance == 0)
        numFramesToAdvance = 1;

    lastFrameTime = SDL_GetTicks();"""
new = """#ifdef FITD_R36S_RELEASE
    // R36S_RELEASE_TIMEBASE: render as often as possible while the clock
    // used for animations, chronometers and scripted delays advances at
    // 25 ticks per second of real elapsed time (not per drawn frame).
    static const bool r36sRealTimebase = []() {
        const char* mode = SDL_getenv("FITD_R36S_TIMEBASE");
        return !mode || SDL_strcmp(mode, "legacy") != 0;
    }();
    u32 numFramesToAdvance = 0;
    if (r36sRealTimebase) {
        static Uint64 previousMs = SDL_GetTicks();
        static Uint64 carry25 = 0;
        const Uint64 nowMs = SDL_GetTicks();
        Uint64 elapsedMs = nowMs >= previousMs ? nowMs - previousMs : 0;
        previousMs = nowMs;
        // Avoid spiralling forward after long pauses/suspend/resume.
        if (elapsedMs > 250) elapsedMs = 250;
        carry25 += elapsedMs * 25;
        numFramesToAdvance = static_cast<u32>(carry25 / 1000);
        carry25 %= 1000;
    } else {
        numFramesToAdvance = (SDL_GetTicks() - lastFrameTime) / (1000 / FRAMES_PER_SECOND);
        if (numFramesToAdvance == 0) numFramesToAdvance = 1;
        lastFrameTime = SDL_GetTicks();
    }
#else
    u32 numFramesToAdvance = (SDL_GetTicks() - lastFrameTime) / (1000 / FRAMES_PER_SECOND);
    if (numFramesToAdvance == 0) numFramesToAdvance = 1;
    lastFrameTime = SDL_GetTicks();
#endif"""
if s.count(old) != 1:
    raise RuntimeError(f"{p}: expected one original frame-tick block; found {s.count(old)}")
s = s.replace(old, new, 1)
p.write_text(s)
print("Release: default realtime 25Hz clock independent from render cap")

# Honor the launcher VSync choice in bgfx as well as SDL, to avoid
# a second synchronization point even when SDL swap interval is zero.
p = Path(sys.argv[1]) / "FitdLib/bgfxGlue.cpp"
s = p.read_text()
old = "    initparam.resolution.reset = BGFX_RESET_VSYNC;"
new = """    const char* r36sVsync = SDL_getenv("FITD_VSYNC");
    initparam.resolution.reset =
        (r36sVsync && r36sVsync[0] == '1') ? BGFX_RESET_VSYNC : BGFX_RESET_NONE;"""
if s.count(old) != 1:
    raise RuntimeError(f"{p}: expected one bgfx vsync anchor, found {s.count(old)}")
s = s.replace(old, new, 1)
p.write_text(s)
print("Release: bgfx VSync disabled by default; FITD_VSYNC=1 restores it")
