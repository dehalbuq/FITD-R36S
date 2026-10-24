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
    '#if BGFX_SHADER_LANGUAGE_GLSL >= 300\n'
    'precision highp usampler2D;\n'
    '#endif\n'
    'USAMPLER2D(s_paletteTexture, 1);')
print("R36S shader precision fix installed")

print("R36S diagnostic instrumentation installed")


# R36S runtime findings: KMSDRM pageflip returns EINVAL on the RK3326 kernel.
# Keep SDL_GL_SwapWindow: bgfx sees an externally owned EGL context and its
# GlContext::swap() does not present that SDL-owned surface.
# First request supported vsync behavior instead of KMS async pageflips.
replace_one("FitdLib/osystemSDL.cpp",
    '    int glMajor = 0, glMinor = 0;',
    '    if (!SDL_GL_SetSwapInterval(1)) {'
    ' fprintf(stderr, "[FITD-R36S] warning: cannot enable swap interval 1: %s\\n", SDL_GetError());'
    ' } else {'
    ' fprintf(stderr, "[FITD-R36S] stage=SDL swap interval=1\\n");'
    ' }\n'
    '    int glMajor = 0, glMinor = 0;')

# On this kernel drmModeSetCrtc succeeds but drmModePageFlip fails with -EINVAL.
# SDL 3.2.14's KMSDRM error path only logs and continues, leaving the display
# stuck on its first framebuffer. Use a conservative modeset fallback per frame.
# This costs extra modeset work/possible tearing but enables first visual tests.
replace_one("ThirdParty/SDL/src/video/kmsdrm/SDL_kmsdrmopengles.c",
    '''        } else {
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not queue pageflip: %d", ret);
        }''',
    '''        } else if (ret == -EINVAL) {
            static bool r36s_pageflip_fallback_logged = false;
            if (!r36s_pageflip_fallback_logged) {
                SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
                    "FITD-R36S: drmModePageFlip returned -EINVAL; switching to drmModeSetCrtc fallback");
                r36s_pageflip_fallback_logged = true;
            }
            ret = KMSDRM_drmModeSetCrtc(viddata->drm_fd,
                                        dispdata->crtc->crtc_id,
                                        fb_info->fb_id, 0, 0,
                                        &dispdata->connector->connector_id, 1,
                                        &dispdata->mode);
            if (ret != 0) {
                return SDL_SetError("FITD-R36S: drmModeSetCrtc fallback failed (%d)", ret);
            }
            windata->waiting_for_flip = false;
        } else {
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not queue pageflip: %d", ret);
        }''')

# Avoid unsigned-vector % in the noise shader. On the Mali GLES compiler the
# translation can become a mod() call with integer arguments, which GLSL ES
# does not overload; use floor-based wrapping instead.
replace_one("FitdLib/shaders/noise_ps.sc",
    '    uvec2 noiseSampler = uvec2(v_screenSpacePosition.xy * v_screenSpacePosition.z) % uvec2(255, 255);',
    '''    vec2 noisePosition = floor(v_screenSpacePosition.xy * v_screenSpacePosition.z);
    vec2 noiseWrapped = noisePosition - floor(noisePosition / 255.0) * 255.0;
    ivec2 noiseSampler = ivec2(noiseWrapped);''')
print("R36S KMSDRM EINVAL fallback and noise shader fix installed")


# Dump the *actual* final GLSL ES source fed into glShaderSource when Mali
# reports L0001. Shader compile errors alone only report line numbers, not
# which injected macro or source fragment caused those lines.
replace_one("ThirdParty/bgfx.cmake/bgfx/src/renderer_gl.cpp",
    '#include "bgfx_p.h"',
    '#include "bgfx_p.h"\n#if defined(FITD_R36S)\n#include <stdio.h>\n#endif')
replace_one("ThirdParty/bgfx.cmake/bgfx/src/renderer_gl.cpp",
    'if (0 == compiled)\n\t\t\t{\n\t\t\t\tbx::LineReader lineReader(code);',
    'if (0 == compiled)\n\t\t\t{\n'
    '#if defined(FITD_R36S)\n'
    '                fprintf(stderr, "[FITD-R36S] FATAL: GPU rejected GLSL shader type=0x%x bytes=%d\\n",'
    ' (unsigned)m_type, (int)code.getLength());\n'
    '                bx::LineReader r36sShaderLines(code);\n'
    '                for (int r36sLine = 1; !r36sShaderLines.isDone(); ++r36sLine) {\n'
    '                    bx::StringView r36sText = r36sShaderLines.next();\n'
    '                    fprintf(stderr, "[R36S-GLSL] %04d | %.*s\\n",'
    ' r36sLine, r36sText.getLength(), r36sText.getPtr());\n'
    '                }\n'
    '                fflush(stderr);\n'
    '#endif\n'
    '\t\t\t\tbx::LineReader lineReader(code);')
print("R36S GLSL source diagnostics installed")


# Debug trace isolated the remaining Mali shader failure to sphere_ps.sc:
# glsl-optimizer corrupted switch(material) into invalid GLSL such as
# "(0 == tmpvar_7) tmpvar_8 = bool(1);".
# Preserve the original material effects without a switch/case statement,
# and replace cos(asin(y)) with sqrt(1-y*y) inside the rendered hemisphere.
replace_one("FitdLib/shaders/sphere_ps.sc",
    '''    switch(material) {
    case 0: // flat
        gl_FragColor = getColor(bank, color);
        break;
    case 2: //transparent
        gl_FragColor = getColor(bank, color);
        gl_FragColor.w = 0.5f;
        break;
    case 3: //marbre
        float angle = asin(normalizedPosition.y);
        distanceToCircleOnScanline = ((normalizedPosition.x / cos(angle)) / 2.f) + 0.5f;
        color = int((distanceToCircleOnScanline) * 15.f);
        gl_FragColor = getColor(bank, color);
        break;
    default:
        gl_FragColor = getColor(bank, color);
        break;
    }''',
    '''    // Mali GLES / glsl-optimizer does not safely translate GLSL switch here.
    if (material == 3) {
        float circleHalfWidth = sqrt(max(1.0 - normalizedPosition.y * normalizedPosition.y, 0.0001));
        distanceToCircleOnScanline = (normalizedPosition.x / circleHalfWidth) * 0.5 + 0.5;
        color = int(distanceToCircleOnScanline * 15.0);
    }
    vec4 resultColor = getColor(bank, color);
    if (material == 2) {
        resultColor.w = 0.5;
    }
    gl_FragColor = resultColor;''')
print("R36S sphere shader GLSL switch removal installed")



# Hardware isolation test: draw primary RGB colors using raw GLES on SDL's
# own framebuffer before launching bgfx/the game. This distinguishes scanout
# failures from issues in the bgfx/game rendering path. Off: FITD_VIDEO_TEST=0.
replace_one("FitdLib/osystemSDL.cpp",
    '    int glMajor = 0, glMinor = 0;',
    '''    const char* r36sVideoTest = std::getenv("FITD_VIDEO_TEST");
    if (!r36sVideoTest || r36sVideoTest[0] != '0') {
        using R36sClearColor = void (*)(float, float, float, float);
        using R36sClear = void (*)(unsigned int);
        R36sClearColor setColor = reinterpret_cast<R36sClearColor>(SDL_GL_GetProcAddress("glClearColor"));
        R36sClear clearBuffer = reinterpret_cast<R36sClear>(SDL_GL_GetProcAddress("glClear"));
        if (setColor && clearBuffer) {
            fprintf(stderr, "[FITD-R36S] VIDEO TEST: red, green, blue; expect 3 visible colors\\n");
            const float colors[3][3] = {{1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}};
            const char* colorNames[3] = {"RED", "GREEN", "BLUE"};
            for (int i = 0; i < 3; ++i) {
                setColor(colors[i][0], colors[i][1], colors[i][2], 1.f);
                clearBuffer(0x00004000u); // GL_COLOR_BUFFER_BIT
                if (!SDL_GL_SwapWindow(gWindowBGFX)) {
                    fprintf(stderr, "[FITD-R36S] VIDEO TEST: swap failed %s\\n", SDL_GetError());
                }
                fprintf(stderr, "[FITD-R36S] VIDEO TEST: submitted %s\\n", colorNames[i]);
                SDL_Delay(650);
            }
            fprintf(stderr, "[FITD-R36S] VIDEO TEST: finished; starting game\\n");
        } else {
            fprintf(stderr, "[FITD-R36S] VIDEO TEST: GLES glClear functions unavailable\\n");
        }
    }
    int glMajor = 0, glMinor = 0;''')

# Make START + SELECT held for 1.5s an explicit emergency-exit combination.
# Also accept Enter + Escape when a firmware maps handheld buttons as keys.
# The port's original gamepad code runs before the FITD_DEBUGGER marker.
replace_one("FitdLib/input.cpp",
    '#include <backends/imgui_impl_sdl3.h>',
    '#include <backends/imgui_impl_sdl3.h>\n#include <cstdio>\n#include <cstdlib>')
replace_one("FitdLib/input.cpp",
    '#ifdef FITD_DEBUGGER\n    if (debuggerVar_topCamera)',
    '''#ifdef FITD_R36S
    static bool r36sReportedDevices = false;
    if (!r36sReportedDevices) {
        int rawJoysticks = 0, gamepads = 0;
        SDL_JoystickID* joys = SDL_GetJoysticks(&rawJoysticks);
        SDL_JoystickID* pads = SDL_GetGamepads(&gamepads);
        fprintf(stderr, "[FITD-R36S] input devices: joysticks=%d mapped gamepads=%d\\n", rawJoysticks, gamepads);
        SDL_free(joys);
        SDL_free(pads);
        r36sReportedDevices = true;
    }
    const bool* r36sKeys = SDL_GetKeyboardState(nullptr);
    const bool r36sKeyQuit = r36sKeys && r36sKeys[SDL_SCANCODE_RETURN] && r36sKeys[SDL_SCANCODE_ESCAPE];
    const bool r36sPadQuit = pad &&
        SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_START) &&
        SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_BACK);
    static Uint64 r36sQuitSince = 0;
    if (r36sPadQuit || r36sKeyQuit) {
        if (r36sQuitSince == 0) r36sQuitSince = SDL_GetTicks();
        if (SDL_GetTicks() - r36sQuitSince >= 1500) {
            fprintf(stderr, "[FITD-R36S] EXIT: START+SELECT (or Enter+Escape) held\\n");
            fflush(stderr);
            std::_Exit(0);
        }
    } else {
        r36sQuitSince = 0;
    }
#endif
#ifdef FITD_DEBUGGER
    if (debuggerVar_topCamera)''')
replace_one("FitdLib/input.cpp",
    '        case SDL_EVENT_QUIT:',
    '''        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            fprintf(stderr, "[FITD-R36S] gamepad button pressed: %d\\n", (int)event.gbutton.button);
            break;
        case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
            fprintf(stderr, "[FITD-R36S] joystick button pressed: %d\\n", (int)event.jbutton.button);
            break;
        case SDL_EVENT_QUIT:''')

# Report frame pacing and whether actual SDL buffer swaps are successful.
replace_one("FitdLib/bgfxGlue.cpp",
    '    if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "kmsdrm") == 0)\n        SDL_GL_SwapWindow(gWindowBGFX);',
    '''    if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "kmsdrm") == 0) {
        if (!SDL_GL_SwapWindow(gWindowBGFX)) {
            fprintf(stderr, "[FITD-R36S] SDL_GL_SwapWindow failed: %s\\n", SDL_GetError());
        }
        static unsigned int r36sFrameCounter = 0;
        ++r36sFrameCounter;
        if (r36sFrameCounter % 125u == 0u) {
            fprintf(stderr, "[FITD-R36S] game rendered %u frames; GL context=%p\\n",
                r36sFrameCounter, SDL_GL_GetCurrentContext());
        }
    }''')
print("R36S video scanout test + emergency exit + frame diagnostics installed")



# GPU readback and presentation isolation test. The already successful RGB
# startup test proves SDL/EGL/KMSDRM can display pixels in the initial context.
# This diagnostic checks whether bgfx actually submits draw calls and whether
# pixel data survives to the SDL-owned window surface after bgfx::frame().
# A three-second magenta fullscreen probe is drawn *after* bgfx::frame(),
# allowing device testing without disassembling commercial game files.
replace_one("FitdLib/bgfxGlue.cpp",
    'void EndFrame()',
    '''#ifdef FITD_R36S
static void r36sInspectFrameBeforeSwap()
{
    static Uint64 firstFrameAt = 0;
    static unsigned frame = 0;
    if (firstFrameAt == 0) firstFrameAt = SDL_GetTicks();
    ++frame;

    const bgfx::Stats* stats = bgfx::getStats();
    if (frame == 1 || frame % 125 == 0) {
        fprintf(stderr, "[FITD-R36S] BGFX STATS: frames=%u draws=%u compute=%u views=%u textures=%u programs=%u output=%ux%u\\n",
            frame,
            stats ? stats->numDraw : 0,
            stats ? stats->numCompute : 0,
            stats ? stats->numViews : 0,
            stats ? stats->numTextures : 0,
            stats ? stats->numPrograms : 0,
            stats ? stats->width : 0,
            stats ? stats->height : 0);
    }

    using GetInt = void (*)(unsigned, int*);
    using GetFloat = void (*)(unsigned, float*);
    using BindFramebuffer = void (*)(unsigned, unsigned);
    using ClearColor = void (*)(float, float, float, float);
    using Clear = void (*)(unsigned);
    using ReadPixels = void (*)(int, int, int, int, unsigned, unsigned, void*);
    using IsEnabled = unsigned char (*)(unsigned);
    using EnableDisable = void (*)(unsigned);
    using GetError = unsigned (*)();

    GetInt glGetInteger = reinterpret_cast<GetInt>(SDL_GL_GetProcAddress("glGetIntegerv"));
    GetFloat glGetFloat = reinterpret_cast<GetFloat>(SDL_GL_GetProcAddress("glGetFloatv"));
    BindFramebuffer glBindFB = reinterpret_cast<BindFramebuffer>(SDL_GL_GetProcAddress("glBindFramebuffer"));
    ClearColor glSetColor = reinterpret_cast<ClearColor>(SDL_GL_GetProcAddress("glClearColor"));
    Clear glClearBuffer = reinterpret_cast<Clear>(SDL_GL_GetProcAddress("glClear"));
    ReadPixels glRead = reinterpret_cast<ReadPixels>(SDL_GL_GetProcAddress("glReadPixels"));
    IsEnabled glIsEnabled = reinterpret_cast<IsEnabled>(SDL_GL_GetProcAddress("glIsEnabled"));
    EnableDisable glDisable = reinterpret_cast<EnableDisable>(SDL_GL_GetProcAddress("glDisable"));
    EnableDisable glEnable = reinterpret_cast<EnableDisable>(SDL_GL_GetProcAddress("glEnable"));
    GetError glGetError = reinterpret_cast<GetError>(SDL_GL_GetProcAddress("glGetError"));

    if (!glGetInteger || !glBindFB || !glRead || !glSetColor || !glClearBuffer)
        return;

    int fbo = 0;
    glGetInteger(0x8CA6u, &fbo); // GL_FRAMEBUFFER_BINDING
    bool probe = SDL_GetTicks() - firstFrameAt < 3500u;
    bool sample = probe && (frame == 1 || frame == 25 || frame == 75);
    sample = sample || (frame % 250 == 0);

    if (sample) {
        unsigned char before[4] = {0};
        // Switch to the SDL window's default framebuffer for direct sampling.
        glBindFB(0x8D40u, 0u); // GL_FRAMEBUFFER
        glRead(320, 240, 1, 1, 0x1908u, 0x1401u, before); // GL_RGBA, GL_UNSIGNED_BYTE
        fprintf(stderr, "[FITD-R36S] PIXEL BEFORE MAGENTA: frame=%u fbo=%d RGBA=%u,%u,%u,%u glerr=0x%x\\n",
            frame, fbo, before[0], before[1], before[2], before[3], glGetError ? glGetError() : 0u);
    }
    if (probe) {
        float original[4] = {0.f, 0.f, 0.f, 1.f};
        if (glGetFloat) glGetFloat(0x0C22u, original); // GL_COLOR_CLEAR_VALUE
        const bool hadScissor = glIsEnabled && glIsEnabled(0x0C11u); // GL_SCISSOR_TEST
        if (glDisable) glDisable(0x0C11u);
        glBindFB(0x8D40u, 0u);
        glSetColor(1.f, 0.f, 1.f, 1.f);
        glClearBuffer(0x00004000u); // GL_COLOR_BUFFER_BIT
        if (frame == 1 || frame == 25 || frame == 75) {
            unsigned char after[4] = {0};
            glRead(320, 240, 1, 1, 0x1908u, 0x1401u, after);
            fprintf(stderr, "[FITD-R36S] MAGENTA PRESENTATION TEST: frame=%u expected magenta RGBA=%u,%u,%u,%u glerr=0x%x\\n",
                frame, after[0], after[1], after[2], after[3], glGetError ? glGetError() : 0u);
        }
        glSetColor(original[0], original[1], original[2], original[3]);
        if (hadScissor && glEnable) glEnable(0x0C11u);
    }
    glBindFB(0x8D40u, static_cast<unsigned>(fbo));
}
#endif

void EndFrame()''')
replace_one("FitdLib/bgfxGlue.cpp",
    '        if (!SDL_GL_SwapWindow(gWindowBGFX)) {',
    '        r36sInspectFrameBeforeSwap();\n        if (!SDL_GL_SwapWindow(gWindowBGFX)) {')
replace_one("FitdLib/input.cpp",
    '''    const bool r36sPadQuit = pad &&
        SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_START) &&
        SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_BACK);''',
    '''    const bool r36sPadQuit = pad &&
        ((SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_START) &&
          SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_BACK)) ||
         (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_SOUTH) &&
          SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_EAST)));''')
replace_one("FitdLib/input.cpp",
    'EXIT: START+SELECT (or Enter+Escape) held',
    'EXIT: START+SELECT or A+B (or Enter+Escape) held')
print("R36S post-bgfx pixel probe / draw stats / A+B exit installed")
