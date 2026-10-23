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
