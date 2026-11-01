#!/usr/bin/env python3
"""Experimental ARMv7 Android cross port; no R36S Linux debug patches."""
from pathlib import Path
import sys
if len(sys.argv)!=3: raise SystemExit("usage: prepare_android.py SOURCE ANDROID_GRADLE")
root=Path(sys.argv[1]);app=Path(sys.argv[2])
def edit(path,old,new,name):
    p=root/path;s=p.read_text()
    if s.count(old)!=1:raise RuntimeError(f"{name}: expected 1 marker, found {s.count(old)}")
    p.write_text(s.replace(old,new,1));print("ARMV7:",name)

edit("CMakeLists.txt",
'set(SDL_SHARED OFF CACHE BOOL "" FORCE)\nset(SDL_STATIC ON CACHE BOOL "" FORCE)',
'''if(ANDROID)
  set(SDL_SHARED ON CACHE BOOL "" FORCE)
  set(SDL_STATIC OFF CACHE BOOL "" FORCE)
else()
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
endif()''',"build shared SDL3 for Android")

# Android Gradle AGP uses CMake's RelWithDebInfo even in :assembleRelease.
# Explicitly prevent any -g code generation in every native dependency and
# keep full compiler optimization for 32-bit ARM, without unsafe fast-math.
edit("CMakeLists.txt",
'project(FITD)',
'''project(FITD)
if(ANDROID)
  set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 -DNDEBUG -g0 -ffunction-sections -fdata-sections" CACHE STRING "" FORCE)
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -DNDEBUG -g0 -ffunction-sections -fdata-sections" CACHE STRING "" FORCE)
  set(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "-Wl,--gc-sections -Wl,--strip-debug" CACHE STRING "" FORCE)
endif()''',"strict no-debug Release code generation")

# Native-host shader compiler is run once before Android cross-compilation.
# Do not build a *target-ARM* shaderc executable (cannot run on CI x86).
edit("CMakeLists.txt",
'set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)',
'''set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
if(ANDROID)
  set(BGFX_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
endif()''',"skip shaderc target on Android")

p=root/"FitdLib/CMakeLists.txt";s=p.read_text()
needle="macro(addShaderProgramPermutation vsname psname varyingname)"
first=s.find(needle)
last=s.find("endmacro()",first)
if first<0 or last<=first:raise RuntimeError("shader compilation macro missing")
s=s[:first]+s[first:last].replace(needle,
    needle+"\n    if(NOT ANDROID)",1)+"    endif()\n"+s[last:]
p.write_text(s)
print("ARMV7: reuse native host-generated GLES3 shader headers")

edit("Fitd/CMakeLists.txt",
'''add_executable (Fitd ${SOURCES})

TARGET_LINK_LIBRARIES(Fitd FitdLib bgfx zlibstatic SDL3-static soloud ${EXTRA_LIBS})''',
'''if(ANDROID)
    add_library(main SHARED ${SOURCES})
    set(FITD_TARGET main)
    target_compile_definitions(main PRIVATE FITD_ANDROID_ARMV7_RELEASE=1)
    target_link_libraries(main PRIVATE FitdLib bgfx zlibstatic SDL3-shared soloud
                          ${EXTRA_LIBS} android log EGL GLESv3)
else()
    add_executable(Fitd ${SOURCES})
    set(FITD_TARGET Fitd)
    target_link_libraries(Fitd PRIVATE FitdLib bgfx zlibstatic SDL3-static soloud
                          ${EXTRA_LIBS})
endif()''',"compile libmain.so JNI instead of ELF")
edit("Fitd/CMakeLists.txt",
'set_target_properties(Fitd PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY',
'set_target_properties(${FITD_TARGET} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY',"fix target name")
edit("Fitd/CMakeLists.txt",
'set_target_properties(Fitd PROPERTIES WIN32_EXECUTABLE FALSE)',
'set_target_properties(${FITD_TARGET} PROPERTIES WIN32_EXECUTABLE FALSE)',"target cross platform")

edit("Fitd/fitd.cpp",
'int main(int argc, char* argv[])\n{',
'''#ifdef __ANDROID__
extern "C" __attribute__((visibility("default"))) int SDL_main(int argc,char* argv[])
#else
int main(int argc,char* argv[])
#endif
{''',"SDL JNI entry symbol")

edit("Fitd/fitd.cpp",
'''    FitdInit(argc, argv);
    FitdMain(argc, argv);''',
'''#ifdef __ANDROID__
    // Avoid worker call after GLES3 or data-file initialization fails.
    return FitdInit(argc,argv);
#else
    FitdInit(argc, argv);
    FitdMain(argc, argv);
#endif''',"propagate SDL init failure cleanly")

edit("FitdLib/osystemSDL.cpp",
'#include "osystemAL.h"',
'''#include "osystemAL.h"
#ifdef __ANDROID__
#include <SDL3/SDL_system.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#endif''',"Android storage helpers")
edit("FitdLib/osystemSDL.cpp",'char homePath[256] = "";','char homePath[512] = "";',"512-byte game directory")
edit("FitdLib/osystemSDL.cpp",
'    int FitdMain(void* unkused);',
'''#ifdef __ANDROID__
    int FitdMain(int argc,char* argv[]);
#else
    int FitdMain(void* unkused);
#endif''',"thread entry signatures")
edit("FitdLib/osystemSDL.cpp",
'SDL_Semaphore* startOfRender = NULL;',
'''#ifdef __ANDROID__
static SDL_GLContext androidGLES=nullptr;
static int SDLCALL androidGameThread(void*) {
    if(!SDL_GL_MakeCurrent(gWindowBGFX,androidGLES)) return 8;
    return FitdMain(0,nullptr);
}
#endif
SDL_Semaphore* startOfRender = NULL;''',"move GLES context to native worker")
edit("FitdLib/osystemSDL.cpp",
'    flags |= SDL_WINDOW_RESIZABLE;',
'''#ifdef __ANDROID__
    flags |= SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
#else
    flags |= SDL_WINDOW_RESIZABLE;
#endif''',"GLES3 fullscreen Android")
edit("FitdLib/osystemSDL.cpp",
'    gWindowBGFX = SDL_CreateWindow("FITD", resolution[0], resolution[1], flags);',
'''    gWindowBGFX = SDL_CreateWindow("FITD", resolution[0], resolution[1], flags);
#ifdef __ANDROID__
    if(!gWindowBGFX) return 9;
    androidGLES=SDL_GL_CreateContext(gWindowBGFX);
    if(!androidGLES) return 10;
    const char* storage=SDL_GetAndroidInternalStoragePath();
    if(!storage || chdir(storage)!=0) return 11;
    snprintf(homePath,sizeof(homePath),"%s/",storage);
    if(!fileExists("LISTBOD2.PAK")) return 12;
    SDL_GL_MakeCurrent(gWindowBGFX,nullptr);
#endif''',"private Android game directory and GL context")
edit("FitdLib/osystemSDL.cpp",
'    SDL_CreateThread(FitdMain, "FitdMainThread", NULL);',
'''#ifdef __ANDROID__
    if(!SDL_CreateThread(androidGameThread,"FitdAndroid",NULL)) return 13;
#else
    SDL_CreateThread(FitdMain, "FitdMainThread", NULL);
#endif''',"Android worker creation")
edit("FitdLib/osystemSDL.cpp",
'if (!SDL_Init(SDL_INIT_VIDEO))',
'if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO))',"Bluetooth controls and audio")

edit("FitdLib/bgfxGlue.cpp",
'    createBgfxInitParams();\n    //initparam.type = bgfx::RendererType::OpenGL;',
'''    createBgfxInitParams();
#ifdef __ANDROID__
    initparam.platformData.context=SDL_GL_GetCurrentContext();
    initparam.type=bgfx::RendererType::OpenGLES;
#endif
    //initparam.type = bgfx::RendererType::OpenGL;''',"bgfx Android OpenGLES backend")
p=root/"FitdLib/bgfxGlue.cpp";s=p.read_text();start=s.find('    if (ImGui::BeginMainMenuBar())');end=s.find('    bgfx::setViewRect(',start)
if start<0 or end<=start:raise RuntimeError("debug FPS GUI not found")
s=s[:start]+'#ifndef __ANDROID__\n'+s[start:end]+'#endif\n'+s[end:];p.write_text(s)
print("ARMV7: excluded development overlay in release")

p=root/"FitdLib/input.cpp";s=p.read_text();target='    debuggerVar_fastForward = false;'
if s.count(target)!=1:raise RuntimeError("input hook")
s=s.replace(target,'''#ifdef __ANDROID__
    static SDL_Gamepad* pad=nullptr;
    if(pad && !SDL_GamepadConnected(pad)) {SDL_CloseGamepad(pad);pad=nullptr;}
    if(!pad) {
        int count=0;
        SDL_JoystickID* ids=SDL_GetGamepads(&count);
        if(ids){if(count>0)pad=SDL_OpenGamepad(ids[0]);SDL_free(ids);}
    }
    if(pad) {
        const int dead=12000;
        const Sint16 x=SDL_GetGamepadAxis(pad,SDL_GAMEPAD_AXIS_LEFTX);
        const Sint16 y=SDL_GetGamepadAxis(pad,SDL_GAMEPAD_AXIS_LEFTY);
        if(SDL_GetGamepadButton(pad,SDL_GAMEPAD_BUTTON_DPAD_UP)||y < -dead) JoyD |= 1;
        if(SDL_GetGamepadButton(pad,SDL_GAMEPAD_BUTTON_DPAD_DOWN)||y > dead) JoyD |= 2;
        if(SDL_GetGamepadButton(pad,SDL_GAMEPAD_BUTTON_DPAD_LEFT)||x < -dead) JoyD |= 4;
        if(SDL_GetGamepadButton(pad,SDL_GAMEPAD_BUTTON_DPAD_RIGHT)||x > dead) JoyD |= 8;
        if(SDL_GetGamepadButton(pad,SDL_GAMEPAD_BUTTON_SOUTH)||
           SDL_GetGamepadButton(pad,SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) Click=1;
        if(SDL_GetGamepadButton(pad,SDL_GAMEPAD_BUTTON_START)||
           SDL_GetGamepadButton(pad,SDL_GAMEPAD_BUTTON_EAST)) key=0x1B;
        if(SDL_GetGamepadButton(pad,SDL_GAMEPAD_BUTTON_NORTH)) key=0x1C;
    }
#endif

'''+target,1);p.write_text(s)
print("ARMV7: SDL3 Bluetooth gamepad / D-pad / analog stick mapping")

# JNI shared native target built from its source checkout; no commercial assets.
(app/"app/jni/CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.22)
project(FITDAndroid LANGUAGES C CXX)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../../source" "${CMAKE_BINARY_DIR}/fitd-root")
'''.replace("$","$"))

p=app/"app/build.gradle";g=p.read_text()
g=g.replace("namespace = \"org.libsdl.app\"","namespace = \"org.libsdl.app\"")
g=g.replace("    defaultConfig {","    defaultConfig {\n        applicationId \"com.dehalbuq.fitd\"",1)
g=g.replace("targetSdkVersion 35","targetSdkVersion 28").replace("versionName \"1.0\"","versionName \"0.1.0-armv7\"")
g=g.replace("abiFilters 'arm64-v8a'","abiFilters 'armeabi-v7a'")
g=g.replace("android {","android {\n    ndkVersion \"28.2.13676358\"",1)

# Configure externalNativeBuild with FITD as the top-level CMake project:
# its existing CMakeLists.txt relies on CMAKE_SOURCE_DIR for third-party deps.
g=g.replace("path 'jni/CMakeLists.txt'","path '../../android-source/CMakeLists.txt'")

g=g.replace('''    buildTypes {
        release {''',
'''    signingConfigs {
        releaseLocal {
            storeFile file(System.getenv("FITD_ANDROID_KEYSTORE") ?: "ci-release.keystore")
            storePassword System.getenv("FITD_ANDROID_KEYSTORE_PASSWORD") ?: ""
            keyAlias "fitd"
            keyPassword System.getenv("FITD_ANDROID_KEY_PASSWORD") ?: ""
        }
    }
    buildTypes {
        release {
            signingConfig signingConfigs.releaseLocal
            debuggable false''')
if "signingConfig signingConfigs.releaseLocal" not in g:raise RuntimeError("Gradle signing hook")
p.write_text(g)
manifest=app/"app/src/main/AndroidManifest.xml";m=manifest.read_text()
m=m.replace('android:glEsVersion="0x00020000"','android:glEsVersion="0x00030000"')
m=m.replace('<activity android:name="SDLActivity"','<activity android:name="org.libsdl.app.SDLActivity"',1)
m=m.replace('android:exported="true"','android:exported="false"',1)
launcher='''            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>'''
if m.count(launcher)!=1:raise RuntimeError("manifest launcher marker")
m=m.replace(launcher,'',1)
marker='<activity android:name="org.libsdl.app.SDLActivity"'
m=m.replace(marker,'''<activity android:name="org.libsdl.app.FITDImportActivity"
            android:exported="true" android:label="FITD ARMv7">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>
        '''+marker,1)
manifest.write_text(m)
strings=app/"app/src/main/res/values/strings.xml";ss=strings.read_text().replace('name="app_name">','name="app_name">FITD Android ARMv7 - ',1);strings.write_text(ss)
print("ARMV7: Gradle/manifest configured for non-debuggable release, Android 5+")
