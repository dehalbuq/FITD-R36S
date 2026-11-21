#!/usr/bin/env python3
"""Adapt SDL3/BGFX to H36S PowerVR fbdev using pinned Handkit MIT backend."""
from pathlib import Path
import shutil, sys
if len(sys.argv) != 3:
    raise SystemExit("usage: patch_h36s_platform.py FITD_SOURCE HANDKIT_SOURCE")
r,h=Path(sys.argv[1]),Path(sys.argv[2])
def change(path,old,new,n=1):
    p=r/path
    s=p.read_text()
    if new in s: return
    if s.count(old)!=n: raise SystemExit(f"H36S patch guard: {path}: expected {n} occurrences, got {s.count(old)}")
    p.write_text(s.replace(old,new,n))
video=h/"src/platform/sdl_handkit_video.c"
if not video.is_file() or "VideoBootStrap PRIVATE_bootstrap" not in video.read_text():
    raise SystemExit("Pinned Handkit SDL3 video source missing")
shutil.copyfile(video,r/"ThirdParty/SDL/src/video/fitd_h36s_handkit_video.c")
change("CMakeLists.txt",
    "add_subdirectory( \x24{THIRD_PARTY}/SDL )",
    """add_subdirectory( \x24{THIRD_PARTY}/SDL )
# Handkit MIT framebuffer PowerVR / A133P SDL3 video.
target_sources(SDL3-static PRIVATE
  "\x24{THIRD_PARTY}/SDL/src/video/fitd_h36s_handkit_video.c")
target_compile_definitions(SDL3-static PRIVATE SDL_VIDEO_DRIVER_PRIVATE=1)""")
change("FitdLib/osystemSDL.cpp",
    "    resolution[0] = 640;\n    resolution[1] = 480;",
    """    const SDL_DisplayMode* h36sDisplay = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());
    if (!h36sDisplay) {
        fprintf(stderr, "[H36S] failed to query native display: %s\\n", SDL_GetError());
        return 1;
    }
    resolution[0] = h36sDisplay->w;
    resolution[1] = h36sDisplay->h;
    fprintf(stderr, "[H36S] native screen %dx%d\\n", resolution[0], resolution[1]);""")
change("FitdLib/bgfxGlue.cpp",
    "    initparam.resolution.width = 640;\n    initparam.resolution.height = 480;",
    """    int h36sWidth = 640, h36sHeight = 480;
    SDL_GetWindowSize(gWindowBGFX, &h36sWidth, &h36sHeight);
    initparam.resolution.width = h36sWidth;
    initparam.resolution.height = h36sHeight;""")
change("FitdLib/bgfxGlue.cpp",
    'SDL_strcmp(SDL_GetCurrentVideoDriver(), "kmsdrm") == 0',
    'SDL_strcmp(SDL_GetCurrentVideoDriver(), "handkit") == 0')
change("FitdLib/bgfxGlue.cpp",
    "    initparam.resolution.reset = BGFX_RESET_VSYNC;",
    "    initparam.resolution.reset = 0;")
print("[H36S] SDL3 Handkit PowerVR / GLES2 platform patch PASS")
