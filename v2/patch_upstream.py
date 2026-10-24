#!/usr/bin/env python3
"""Pinned yaz0r/FITD experimental port changes for AArch64 + SDL3 + GLES3.
Stop instead of silently patching the wrong version.
"""
import re
import sys
from pathlib import Path

root = Path(sys.argv[1]).resolve()

def replace_one(path, before, after):
    file = root / path
    value = file.read_text()
    matches = value.count(before)
    if matches != 1:
        raise RuntimeError(f"{path}: expected one match, found {matches}: {before[:80]!r}")
    file.write_text(value.replace(before, after, 1))
    print("patched", path)

replace_one("Fitd/fitd.cpp",
    '    FitdInit(argc, argv);\n    FitdMain(argc, argv);\n    return 0;',
    '#ifdef FITD_R36S\n    return FitdInit(argc, argv);\n#else\n'
    '    FitdInit(argc, argv);\n    FitdMain(argc, argv);\n    return 0;\n#endif')

replace_one("FitdLib/osystemSDL.cpp",
    '    int FitdMain(void* unkused);', '    int FitdMain(int argc, char* argv[]);')
replace_one("FitdLib/osystemSDL.cpp",
    'SDL_Semaphore* startOfRender = NULL;',
    'SDL_GLContext gR36sContext = nullptr;\n'
    'static int SDLCALL r36sGameThread(void*) { return FitdMain(0, nullptr); }\n'
    'SDL_Semaphore* startOfRender = NULL;')
replace_one("FitdLib/osystemSDL.cpp",
    '    gWindowBGFX = SDL_CreateWindow("FITD", resolution[0], resolution[1], flags);',
    '#ifdef FITD_R36S\n'
    '    flags |= SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN;\n'
    '    resolution[0] = 640;\n    resolution[1] = 480;\n'
    '    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);\n'
    '    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);\n'
    '    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);\n'
    '#endif\n'
    '    gWindowBGFX = SDL_CreateWindow("FITD", resolution[0], resolution[1], flags);\n'
    '#ifdef FITD_R36S\n'
    '    if (!gWindowBGFX) { fprintf(stderr, "SDL window: %s\\n", SDL_GetError()); return 1; }\n'
    '    gR36sContext = SDL_GL_CreateContext(gWindowBGFX);\n'
    '    if (!gR36sContext) { fprintf(stderr, "EGL/GLES3: %s\\n", SDL_GetError()); return 1; }\n'
    '    SDL_GL_MakeCurrent(gWindowBGFX, nullptr);\n'
    '#endif')
replace_one("FitdLib/osystemSDL.cpp",
    '    SDL_CreateThread(FitdMain, "FitdMainThread", NULL);',
    '    if (!SDL_CreateThread(r36sGameThread, "FitdMainThread", NULL)) {\n'
    '        fprintf(stderr, "Game thread: %s\\n", SDL_GetError()); return 1;\n'
    '    }')

replace_one("FitdLib/bgfxGlue.cpp",
    '#include "SDL3/SDL.h"',
    '#include "SDL3/SDL.h"\n#include <cstdio>')

replace_one("FitdLib/bgfxGlue.cpp",
    '    createBgfxInitParams();',
    '    createBgfxInitParams();\n'
    '#ifdef FITD_R36S\n'
    '    extern SDL_GLContext gR36sContext;\n'
    '    if (!SDL_GL_MakeCurrent(gWindowBGFX, gR36sContext)) {\n'
    '        fprintf(stderr, "SDL_GL_MakeCurrent: %s\\n", SDL_GetError()); return false;\n'
    '    }\n'
    '    initparam.platformData.context = SDL_GL_GetCurrentContext();\n'
    '    initparam.type = bgfx::RendererType::OpenGLES;\n'
    '    initparam.resolution.width = 640;\n'
    '    initparam.resolution.height = 480;\n'
    '    initparam.resolution.reset = BGFX_RESET_VSYNC;\n'
    '    gFrameLimit = 25;\n'
    '#endif')
replace_one("FitdLib/bgfxGlue.cpp",
    '    bgfx::init(initparam);',
    '    if (!bgfx::init(initparam)) {\n'
    '        fprintf(stderr, "bgfx::init failed\\n"); return false;\n'
    '    }')
replace_one("FitdLib/main.cpp",
    '    initBgfxGlue(argc, argv);',
    '    if (!initBgfxGlue(argc, argv)) return 1;')
replace_one("FitdLib/bgfxGlue.cpp",
    '    bgfx::frame();',
    '    bgfx::frame();\n'
    '#ifdef FITD_R36S\n'
    '    // Tentative SDL KMSDRM scanout/pageflip; verify on device.\n'
    '    if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "kmsdrm") == 0)\n'
    '        SDL_GL_SwapWindow(gWindowBGFX);\n'
    '#endif')

replace_one("FitdLib/shaders/embeddedShadersMacro.h",
    '/*BGFX_EMBEDDED_SHADER_ESSL (bgfx::RendererType::OpenGLES,   _name)*/',
    'BGFX_EMBEDDED_SHADER_ESSL (bgfx::RendererType::OpenGLES,   _name)')
file = root / "FitdLib/shaders/embeddedShaders.cpp"
value = file.read_text()
match = re.search(r"#if BGFX_PLATFORM_SUPPORTS_GLSL\n.*?#endif", value, re.S)
if not match or value.count("#if BGFX_PLATFORM_SUPPORTS_GLSL") != 1:
    raise RuntimeError("GLSL shader include block not found")
essl = match.group(0).replace("BGFX_PLATFORM_SUPPORTS_GLSL", "BGFX_PLATFORM_SUPPORTS_ESSL").replace("/glsl/", "/essl/")
file.write_text(value[:match.start()] + essl + "\n\n" + value[match.start():])

file = root / "FitdLib/CMakeLists.txt"
value = file.read_text()
result = []
rules = 0
prev = None
for line in value.splitlines():
    if "bgfx_compile_shaders(SHADERS " in line:
        kind = "VS" if "TYPE VERTEX" in line else "PS" if "TYPE FRAGMENT" in line else None
        if kind is None or not line.rstrip().endswith(")"):
            raise RuntimeError("Unexpected bgfx_compile_shaders syntax")
        result.append(line.rstrip()[:-1] + " OUT_FILES_VAR FITD_GENERATED_" + kind + ")")
        result.append("    list(APPEND SOURCES $" + "{FITD_GENERATED_" + kind + "})")
        rules += 1
    elif "addShaderProgramPermutation(" in line and line.strip() == prev:
        continue
    else:
        result.append(line)
    prev = line.strip()
if rules != 2:
    raise RuntimeError(f"Expected two shader rules, got {rules}")
file.write_text("\n".join(result) + "\n")

file = root / "FitdLib/input.cpp"
value = file.read_text()
marker = '#ifdef FITD_DEBUGGER\n    if (debuggerVar_topCamera)'
if value.count(marker) != 1:
    raise RuntimeError("Could not locate gamepad insertion point")
gamepad = r"""
#ifdef FITD_R36S
    static SDL_Gamepad* pad = nullptr;
    if (pad && !SDL_GamepadConnected(pad)) {
        SDL_CloseGamepad(pad);
        pad = nullptr;
    }
    if (!pad) {
        int count = 0;
        SDL_JoystickID* ids = SDL_GetGamepads(&count);
        if (ids && count > 0) pad = SDL_OpenGamepad(ids[0]);
        SDL_free(ids);
    }
    if (pad) {
        const int deadzone = 15000;
        const Sint16 ax = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
        const Sint16 ay = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY);
        if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_UP) || ay < -deadzone) JoyD |= 1;
        if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_DOWN) || ay > deadzone) JoyD |= 2;
        if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT) || ax < -deadzone) JoyD |= 4;
        if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) || ax > deadzone) JoyD |= 8;
        if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_SOUTH)) Click = 1;
        if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_EAST) ||
            SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_NORTH)) key = 0x1C;
        if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_START)) key = 0x1B;
    }
#endif
"""
file.write_text(value.replace(marker, gamepad + marker, 1))
print("AArch64 SDL3/GLES3 patch applied:", root)


# Diagnostics apply after the original ARM64 patches above, so failures remain
# source-pinned and do not silently modify unrecognized upstream revisions.
replace_one("FitdLib/osystemSDL.cpp",
    '#include <backends/imgui_impl_sdl3.h>',
    '#include <backends/imgui_impl_sdl3.h>\n#include <cstdio>\n#include <cstdlib>')
replace_one("FitdLib/osystemSDL.cpp",
    '    startOfRender = SDL_CreateSemaphore(0);',
    '    setvbuf(stdout, nullptr, _IONBF, 0);\n'
    '    setvbuf(stderr, nullptr, _IONBF, 0);\n'
    '    fprintf(stderr, "[FITD-R36S] stage=FitdInit started\\n");\n'
    '    startOfRender = SDL_CreateSemaphore(0);')
replace_one("FitdLib/osystemSDL.cpp",
    '    osystem_init();',
    '    fprintf(stderr, "[FITD-R36S] stage=SDL_Init begin\\n");\n'
    '    osystem_init();\n'
    '    fprintf(stderr, "[FITD-R36S] stage=SDL_Init OK video=%s\\n", '
    'SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "none");')
replace_one("FitdLib/osystemSDL.cpp",
    '    if (!SDL_Init(SDL_INIT_VIDEO))',
    '    const char* debugMode = std::getenv("FITD_DEBUG");\n'
    '    if (!debugMode || debugMode[0] != \'0\') {\n'
    '        SDL_SetLogPriorities(SDL_LOG_PRIORITY_DEBUG);\n'
    '        fprintf(stderr, "[FITD-R36S] SDL log priority=DEBUG\\n");\n'
    '    }\n'
    '    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))')
replace_one("FitdLib/osystemSDL.cpp",
    '        assert(0);',
    '        fprintf(stderr, "[FITD-R36S] FATAL: SDL initialization failed\\n");\n'
    '        std::exit(11);')
replace_one("FitdLib/osystemSDL.cpp",
    '    gR36sContext = SDL_GL_CreateContext(gWindowBGFX);',
    '    fprintf(stderr, "[FITD-R36S] stage=SDL_CreateWindow OK 640x480\\n");\n'
    '    gR36sContext = SDL_GL_CreateContext(gWindowBGFX);')
replace_one("FitdLib/osystemSDL.cpp",
    '    SDL_GL_MakeCurrent(gWindowBGFX, nullptr);',
    '    int glMajor = 0, glMinor = 0;\n'
    '    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &glMajor);\n'
    '    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &glMinor);\n'
    '    fprintf(stderr, "[FITD-R36S] stage=SDL_GL_CreateContext OK GLES=%d.%d ctx=%p\\n", '
    'glMajor, glMinor, static_cast<void*>(gR36sContext));\n'
    '    if (!SDL_GL_MakeCurrent(gWindowBGFX, nullptr))\n'
    '        fprintf(stderr, "[FITD-R36S] warning: SDL_GL_MakeCurrent(detach): %s\\n", SDL_GetError());')
replace_one("FitdLib/osystemSDL.cpp",
    '    detectGame();',
    '    fprintf(stderr, "[FITD-R36S] stage=detectGame begin\\n");\n'
    '    detectGame();\n'
    '    fprintf(stderr, "[FITD-R36S] stage=detectGame OK\\n");')
replace_one("FitdLib/osystemSDL.cpp",
    'static int SDLCALL r36sGameThread(void*) { return FitdMain(0, nullptr); }',
    'static int SDLCALL r36sGameThread(void*) {\n'
    '    fprintf(stderr, "[FITD-R36S] stage=FitdMain worker started\\n");\n'
    '    const int rc = FitdMain(0, nullptr);\n'
    '    fprintf(stderr, "[FITD-R36S] stage=FitdMain worker returned rc=%d\\n", rc);\n'
    '    fflush(stderr);\n'
    '    return rc;\n'
    '}')
replace_one("FitdLib/osystemSDL.cpp",
    '    unsigned long int t_start = SDL_GetTicks();',
    '    fprintf(stderr, "[FITD-R36S] stage=SDL event loop starting\\n");\n'
    '    unsigned long int t_start = SDL_GetTicks();')
replace_one("FitdLib/osystemSDL.cpp",
    '        SDL_WaitSemaphore(endOfRender);',
    '        if (!SDL_WaitSemaphoreTimeout(endOfRender, 30000)) {\n'
    '            fprintf(stderr, "[FITD-R36S] FATAL: waited 30 seconds for rendering thread; SDL=%s\\n", '
    'SDL_GetError());\n'
    '            fflush(stderr);\n'
    '            std::exit(12);\n'
    '        }')

replace_one("FitdLib/bgfxGlue.cpp",
    '    createBgfxInitParams();',
    '    fprintf(stderr, "[FITD-R36S] stage=bgfx create parameters\\n");\n'
    '    createBgfxInitParams();')
replace_one("FitdLib/bgfxGlue.cpp",
    '    initparam.platformData.context = SDL_GL_GetCurrentContext();',
    '    fprintf(stderr, "[FITD-R36S] stage=bgfx GLES context current=%p\\n", '
    'SDL_GL_GetCurrentContext());\n'
    '    initparam.platformData.context = SDL_GL_GetCurrentContext();')
replace_one("FitdLib/bgfxGlue.cpp",
    '    if (!bgfx::init(initparam)) {',
    '    fprintf(stderr, "[FITD-R36S] stage=bgfx::init begin\\n");\n'
    '    if (!bgfx::init(initparam)) {')
replace_one("FitdLib/bgfxGlue.cpp",
    '    imguiCreate();',
    '    fprintf(stderr, "[FITD-R36S] stage=bgfx::init OK renderer=%s\\n", '
    'bgfx::getRendererName(bgfx::getRendererType()));\n'
    '    imguiCreate();')
replace_one("FitdLib/bgfxGlue.cpp",
    '    bgfx::frame();\n#ifdef FITD_R36S',
    '    static bool loggedFirstFrame = false;\n'
    '    if (!loggedFirstFrame) fprintf(stderr, "[FITD-R36S] stage=first bgfx::frame begin\\n");\n'
    '    bgfx::frame();\n'
    '    if (!loggedFirstFrame) {\n'
    '        fprintf(stderr, "[FITD-R36S] stage=first bgfx::frame OK\\n");\n'
    '        loggedFirstFrame = true;\n'
    '    }\n'
    '#ifdef FITD_R36S')
# GLSL ES 3.0 requires explicit precision for unsigned integer samplers.
# bgfx's generated ESSL currently declares highp float/int, but does not add
# precision for usampler2D; failing only at runtime on Mali GLES drivers.
# All FITD pixel shaders include palette.sh, so one fix covers every program.
replace_one("FitdLib/shaders/palette.sh",
    'USAMPLER2D(s_paletteTexture, 1);',
    '#if BGFX_SHADER_LANGUAGE_GLSL >= 300\\n'
    'precision highp usampler2D;\\n'
    '#endif\\n'
    'USAMPLER2D(s_paletteTexture, 1);')
print("R36S shader precision fix installed")

print("R36S diagnostic instrumentation installed")
