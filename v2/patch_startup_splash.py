#!/usr/bin/env python3
"""Show FITD-R36S attribution and legal BMPs before bgfx takes the GLES context.

Render before game-thread creation and bgfx initialization, using only SDL3,
a small dynamically loaded OpenGL ES 3.0 shader and BMPs generated at build.
No SDL_image, SDL_ttf, framebuffer shaders, or proprietary assets required.
If images or GLES entry points are unavailable, startup continues normally.
"""
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: patch_startup_splash.py UPSTREAM_SOURCE_DIR")
p=Path(sys.argv[1])/"FitdLib/osystemSDL.cpp"
s=p.read_text()

def one(name,old,new):
    global s
    n=s.count(old)
    if n!=1:
        raise RuntimeError(f"startup splash {name}: expected unique anchor, found {n}")
    s=s.replace(old,new,1)

one("additional headers",
'''#include "osystemAL.h"''',
'''#include "osystemAL.h"
#ifdef FITD_R36S
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#endif''')

splash = r'''
#ifdef FITD_R36S
// Draw the port's own notices before bgfx initializes the GL renderer.
// Uses GLES3 functions resolved through SDL rather than adding a runtime
// GLES library dependency. Never reads or packages Alone in the Dark assets.
static void r36sShowStartupNotices()
{
    const char* mode = std::getenv("FITD_R36S_SPLASH");
    if (mode && (std::strcmp(mode,"off")==0 || std::strcmp(mode,"0")==0)) {
        fprintf(stderr, "[FITD-SPLASH] disabled by user\n");
        return;
    }
    const char* dir = std::getenv("FITD_R36S_SPLASH_DIR");
    if (!dir || !*dir) {
        fprintf(stderr, "[FITD-SPLASH] missing splash asset directory; skip\n");
        return;
    }

    struct GLES {
        unsigned (*createShader)(unsigned) = nullptr;
        void (*shaderSource)(unsigned,int,const char* const*,const int*) = nullptr;
        void (*compileShader)(unsigned) = nullptr;
        void (*getShaderiv)(unsigned,unsigned,int*) = nullptr;
        unsigned (*createProgram)() = nullptr;
        void (*attachShader)(unsigned,unsigned) = nullptr;
        void (*linkProgram)(unsigned) = nullptr;
        void (*getProgramiv)(unsigned,unsigned,int*) = nullptr;
        void (*useProgram)(unsigned) = nullptr;
        int (*getUniformLocation)(unsigned,const char*) = nullptr;
        void (*uniform1i)(int,int) = nullptr;
        void (*deleteShader)(unsigned) = nullptr;
        void (*deleteProgram)(unsigned) = nullptr;
        void (*genTextures)(int,unsigned*) = nullptr;
        void (*bindTexture)(unsigned,unsigned) = nullptr;
        void (*texParameteri)(unsigned,unsigned,int) = nullptr;
        void (*texImage2D)(unsigned,int,int,int,int,int,unsigned,unsigned,const void*) = nullptr;
        void (*pixelStorei)(unsigned,int) = nullptr;
        void (*activeTexture)(unsigned) = nullptr;
        void (*deleteTextures)(int,const unsigned*) = nullptr;
        void (*viewport)(int,int,int,int) = nullptr;
        void (*disable)(unsigned) = nullptr;
        void (*clearColor)(float,float,float,float) = nullptr;
        void (*clear)(unsigned) = nullptr;
        void (*drawArrays)(unsigned,int,int) = nullptr;
    } gl;
#define SPLASH_LOAD(field, GLName) \
    gl.field=reinterpret_cast<decltype(gl.field)>(SDL_GL_GetProcAddress("gl" #GLName))
    SPLASH_LOAD(createShader,CreateShader);
    SPLASH_LOAD(shaderSource,ShaderSource);
    SPLASH_LOAD(compileShader,CompileShader);
    SPLASH_LOAD(getShaderiv,GetShaderiv);
    SPLASH_LOAD(createProgram,CreateProgram);
    SPLASH_LOAD(attachShader,AttachShader);
    SPLASH_LOAD(linkProgram,LinkProgram);
    SPLASH_LOAD(getProgramiv,GetProgramiv);
    SPLASH_LOAD(useProgram,UseProgram);
    SPLASH_LOAD(getUniformLocation,GetUniformLocation);
    SPLASH_LOAD(uniform1i,Uniform1i);
    SPLASH_LOAD(deleteShader,DeleteShader);
    SPLASH_LOAD(deleteProgram,DeleteProgram);
    SPLASH_LOAD(genTextures,GenTextures);
    SPLASH_LOAD(bindTexture,BindTexture);
    SPLASH_LOAD(texParameteri,TexParameteri);
    SPLASH_LOAD(texImage2D,TexImage2D);
    SPLASH_LOAD(pixelStorei,PixelStorei);
    SPLASH_LOAD(activeTexture,ActiveTexture);
    SPLASH_LOAD(deleteTextures,DeleteTextures);
    SPLASH_LOAD(viewport,Viewport);
    SPLASH_LOAD(disable,Disable);
    SPLASH_LOAD(clearColor,ClearColor);
    SPLASH_LOAD(clear,Clear);
    SPLASH_LOAD(drawArrays,DrawArrays);
#undef SPLASH_LOAD

    if (!gl.createShader || !gl.shaderSource || !gl.compileShader ||
        !gl.getShaderiv || !gl.createProgram || !gl.attachShader ||
        !gl.linkProgram || !gl.getProgramiv || !gl.useProgram ||
        !gl.getUniformLocation || !gl.uniform1i || !gl.deleteShader ||
        !gl.deleteProgram || !gl.genTextures || !gl.bindTexture ||
        !gl.texParameteri || !gl.texImage2D || !gl.pixelStorei ||
        !gl.activeTexture || !gl.deleteTextures || !gl.viewport ||
        !gl.disable || !gl.clearColor || !gl.clear || !gl.drawArrays) {
        fprintf(stderr, "[FITD-SPLASH] GLES3 function missing; normal startup\n");
        return;
    }

    const char* vert = R"(#version 300 es
precision highp float;
out vec2 vUV;
void main() {
    vec2 xy = vec2((gl_VertexID == 1) ? 3.0 : -1.0,
                   (gl_VertexID == 2) ? 3.0 : -1.0);
    gl_Position = vec4(xy, 0.0, 1.0);
    vUV = vec2((xy.x + 1.0) * 0.5, 1.0 - (xy.y + 1.0) * 0.5);
})";
    const char* frag = R"(#version 300 es
precision mediump float;
in vec2 vUV;
uniform sampler2D uSplash;
out vec4 color;
void main() {
    color = texture(uSplash, vUV);
})";
    auto compile = [&](unsigned type,const char* source)->unsigned {
        unsigned sh=gl.createShader(type);
        if (!sh) return 0u;
        gl.shaderSource(sh,1,&source,nullptr);
        gl.compileShader(sh);
        int ok=0;
        gl.getShaderiv(sh,0x8B81u,&ok); // GL_COMPILE_STATUS
        if (!ok) {
            fprintf(stderr,"[FITD-SPLASH] GLES shader compilation failed type=%u\n",type);
            gl.deleteShader(sh);
            return 0u;
        }
        return sh;
    };
    const unsigned vs=compile(0x8B31u,vert); // GL_VERTEX_SHADER
    const unsigned fs=compile(0x8B30u,frag); // GL_FRAGMENT_SHADER
    if (!vs || !fs) {
        if (vs) gl.deleteShader(vs);
        if (fs) gl.deleteShader(fs);
        return;
    }
    const unsigned program=gl.createProgram();
    if (!program) {
        gl.deleteShader(vs); gl.deleteShader(fs); return;
    }
    gl.attachShader(program,vs);
    gl.attachShader(program,fs);
    gl.linkProgram(program);
    int linked=0;
    gl.getProgramiv(program,0x8B82u,&linked); // GL_LINK_STATUS
    gl.deleteShader(vs); gl.deleteShader(fs);
    if (!linked) {
        fprintf(stderr,"[FITD-SPLASH] GLES program linking failed; normal startup\n");
        gl.deleteProgram(program);
        return;
    }
    unsigned texture=0;
    gl.genTextures(1,&texture);
    if (!texture) { gl.deleteProgram(program); return; }

    gl.useProgram(program);
    gl.activeTexture(0x84C0u); // GL_TEXTURE0
    gl.bindTexture(0x0DE1u,texture); // GL_TEXTURE_2D
    gl.texParameteri(0x0DE1u,0x2801u,0x2601u); // min filter linear
    gl.texParameteri(0x0DE1u,0x2800u,0x2601u); // mag filter linear
    gl.texParameteri(0x0DE1u,0x2802u,0x812Fu); // clamp S
    gl.texParameteri(0x0DE1u,0x2803u,0x812Fu); // clamp T
    gl.pixelStorei(0x0CF5u,1); // GL_UNPACK_ALIGNMENT
    const int sampler=gl.getUniformLocation(program,"uSplash");
    if (sampler>=0) gl.uniform1i(sampler,0);
    int width=640,height=480;
    SDL_GetWindowSize(gWindowBGFX,&width,&height);
    gl.viewport(0,0,width,height);
    gl.disable(0x0B71u); // GL_DEPTH_TEST
    gl.disable(0x0BE2u); // GL_BLEND
    gl.disable(0x0B44u); // GL_CULL_FACE

    // Gamepads must be opened to receive SDL_EVENT_GAMEPAD_BUTTON_DOWN.
    // Close before launching the game's normal input mapping.
    int splashPadCount = 0;
    SDL_JoystickID* splashIds = SDL_GetGamepads(&splashPadCount);
    SDL_Gamepad* splashGamepad = nullptr;
    if (splashIds) {
        if (splashPadCount > 0)
            splashGamepad = SDL_OpenGamepad(splashIds[0]);
        SDL_free(splashIds);
    }
    fprintf(stderr,"[FITD-SPLASH] input padAvailable=%d open=%d\n",
            splashPadCount,splashGamepad ? 1 : 0);

    const char* names[2] = { "credits.bmp", "legal.bmp" };
    const char* durationVars[2] = {"FITD_R36S_CREDITS_MS","FITD_R36S_LEGAL_MS"};
    const int defaultDurations[2] = { 3500, 9000 };
    for (int slide=0;slide<2;++slide) {
        const std::string path=std::string(dir)+"/"+names[slide];
        SDL_Surface* bmp=SDL_LoadBMP(path.c_str());
        if (!bmp) {
            fprintf(stderr,"[FITD-SPLASH] failed to open %s: %s\n",
                    path.c_str(),SDL_GetError());
            continue;
        }
        SDL_Surface* rgba=SDL_ConvertSurface(bmp,SDL_PIXELFORMAT_RGBA32);
        SDL_DestroySurface(bmp);
        if (!rgba) {
            fprintf(stderr,"[FITD-SPLASH] convert BMP failed: %s\n",SDL_GetError());
            continue;
        }
        // Images are generated in this size. Avoid non-contiguous pixel rows
        // or uploading an unexpectedly large/corrupted image.
        if (rgba->w != 640 || rgba->h != 480 || rgba->pitch != 640*4) {
            fprintf(stderr,"[FITD-SPLASH] invalid BMP geometry %dx%d pitch=%d\n",
                    rgba->w,rgba->h,rgba->pitch);
            SDL_DestroySurface(rgba);
            continue;
        }
        gl.texImage2D(0x0DE1u,0,0x1908,640,480,0,0x1908u,0x1401u,rgba->pixels);
        SDL_DestroySurface(rgba);
        int duration=defaultDurations[slide];
        const char* value=std::getenv(durationVars[slide]);
        if (value && *value) {
            const int parsed=std::atoi(value);
            if (parsed>=1500 && parsed<=20000) duration=parsed;
        }
        fprintf(stderr,"[FITD-SPLASH] SHOW slide=%s ms=%d profile=@dehalbuq\n",
                names[slide],duration);
        const Uint64 start=SDL_GetTicks();
        while ((SDL_GetTicks()-start)<(Uint64)duration) {
            gl.clearColor(0.0f,0.0f,0.0f,1.0f);
            gl.clear(0x00004000u); // GL_COLOR_BUFFER_BIT
            gl.drawArrays(0x0004u,0,3); // GL_TRIANGLES full-screen
            if (!SDL_GL_SwapWindow(gWindowBGFX)) {
                fprintf(stderr,"[FITD-SPLASH] swap failed: %s\n",SDL_GetError());
                break;
            }
            SDL_Event event;
            bool skip=false;
            while (SDL_PollEvent(&event)) {
                if ((event.type==SDL_EVENT_KEY_DOWN ||
                     event.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN ||
                     event.type==SDL_EVENT_JOYSTICK_BUTTON_DOWN) &&
                    (SDL_GetTicks()-start)>=1200u)
                    skip=true;
            }
            if (skip) {
                fprintf(stderr,"[FITD-SPLASH] advance by input\n");
                break;
            }
            SDL_Delay(33); // low-cost static display; no busy polling
        }
    }
    if (splashGamepad) SDL_CloseGamepad(splashGamepad);
    gl.bindTexture(0x0DE1u,0);
    gl.deleteTextures(1,&texture);
    gl.useProgram(0);
    gl.deleteProgram(program);
    fprintf(stderr,"[FITD-SPLASH] notices complete; resume original engine\n");
}
#endif
'''
one("startup routine definition",
'''SDL_GLContext gR36sContext = nullptr;''',
'''SDL_GLContext gR36sContext = nullptr;
''' + splash)

one("show before detaching GL context",
'''    if (!SDL_GL_MakeCurrent(gWindowBGFX, nullptr))''',
'''    r36sShowStartupNotices();
    if (!SDL_GL_MakeCurrent(gWindowBGFX, nullptr))''')

p.write_text(s)
print("FITD-R36S: branded attribution+legal startup slides inserted before bgfx init")
