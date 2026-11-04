#!/usr/bin/env python3
"""Generate Test 07 from verified Test 06 camera code without modifying it.

This preview loads ORIGINAL FITD body models on the already working SDL2
GLES2 background. This is NOT yet the original FitdMain simulation.
"""
import sys
from pathlib import Path

if len(sys.argv) != 3:
    raise SystemExit("usage: patch_scene07.py INPUT_CPP OUTPUT_CPP")
input_file, output_file = map(Path, sys.argv[1:3])
s = input_file.read_text()

def replace_one(old: str, new: str, name: str) -> None:
    global s
    count = s.count(old)
    if count != 1:
        raise RuntimeError(f"{name}: expected 1 match, got {count}")
    s = s.replace(old, new, 1)

replace_one(
    '#include "fitd_gameplay_input_sdl2.h"',
    '#include "fitd_gameplay_input_sdl2.h"\n'
    '#include "fitd_character_preview.h"',
    "model preview header"
)
replace_one(
    '    SDL_Log("FITD-SCENE: SUCCESS - first camera frame from actual original PAK");',
    '    fitd_character::ModelPreview actor;\n'
    '    const bool modelReady = actor.init();\n'
    '    SDL_Log("FITD-MODEL: preview initialized=%d bodyIndex=%d",\n'
    '        (int)modelReady, actor.currentIndex());\n'
    '    SDL_Log("FITD-SCENE: SUCCESS - first camera frame from actual original PAK");',
    "model initialized with verified EGL context"
)
replace_one(
    '        if(pad){\n            Sint16 x=SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_LEFTX);',
    '        // Keep the scene renderer at display refresh rate, but update\n'
    '        // the simple preview-model position on fixed 40ms input ticks.\n'
    '        static Uint32 lastModelTick=0;\n'
    '        if(modelReady && SDL_GetTicks()-lastModelTick>=40){\n'
    '            lastModelTick=SDL_GetTicks();\n'
    '            actor.move(liveInput.joyD);\n'
    '        }\n'
    '        if(pad){\n'
    '            Sint16 x=SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_LEFTX);',
    "fixed 25Hz preview movement, independent of GLES2 draw cadence"
)
replace_one(
    '''            if(left&&!lastLeft)nextCamera(-1);
            if(right&&!lastRight)nextCamera(1);
            if(shoulderL&&!lastL)tryFloor(-1);
            if(shoulderR&&!lastR)tryFloor(1);
            if(a&&!lastA)nextCamera(1);
            if(xbutton&&!lastX)nextCamera(-1);''',
    '''            // D-pad or left stick now moves the 3D preview body
            // through the same JoyD state used by real FITD gameplay.
            if(shoulderL&&!lastL)tryFloor(-1);
            if(shoulderR&&!lastR)tryFloor(1);
            if(a&&!lastA && modelReady)actor.turn(35.f);
            if(xbutton&&!lastX && modelReady)
                actor.select(actor.currentIndex()+1,1);''',
    "model movement and selection controls"
)
replace_one(
    '''                    case SDLK_LEFT:nextCamera(-1);break;
                    case SDLK_RIGHT:nextCamera(1);break;''',
    '''                    case SDLK_LEFT:break; // movement via JoyD
                    case SDLK_RIGHT:break;
                    case SDLK_TAB:
                        if(modelReady)actor.select(actor.currentIndex()+1,1);
                        break;''',
    "keyboard movement and model switch"
)
replace_one(
    '        glDrawArrays(GL_TRIANGLE_STRIP,0,4);\n'
    '        SDL_GL_SwapWindow(window);',
    '        glDrawArrays(GL_TRIANGLE_STRIP,0,4);\n'
    '        if(modelReady) actor.draw();\n'
    '        SDL_GL_SwapWindow(window);',
    "draw real original body polygons above background"
)
replace_one(
    '    glDeleteTextures(1,&texture);glDeleteBuffers(1,&vertexBuffer);\n'
    '    glDeleteProgram(program);glDeleteShader(v);glDeleteShader(f);\n'
    '    SDL_GL_DeleteContext(gl);SDL_DestroyWindow(window);\n'
    '    closePad();SDL_Quit();',
    '    actor.shutdown();\n'
    '    glDeleteTextures(1,&texture);glDeleteBuffers(1,&vertexBuffer);\n'
    '    glDeleteProgram(program);glDeleteShader(v);glDeleteShader(f);\n'
    '    SDL_GL_DeleteContext(gl);SDL_DestroyWindow(window);\n'
    '    closePad();SDL_Quit();',
    "clean preview model GL resources before EGL teardown"
)
output_file.write_text(s)
print("FITD Test 07 generated:",output_file)
