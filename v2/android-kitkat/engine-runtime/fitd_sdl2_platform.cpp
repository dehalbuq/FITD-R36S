/*
 * Android 4.4+ SDL2 platform adaptation of original FITD FitdLib/osystemSDL.cpp.
 *
 * Prepared for integration with the REAL FitdMain() engine, not a simulated
 * game loop. The upstream functions in this file are intentionally provided
 * only on the Android runtime target; don't compile upstream osystemSDL.cpp
 * alongside this translation unit (it defines the same symbols).
 *
 * Rendering backend and full gameplay linkage are separate pending steps.
 */
#include "common.h"
#include "bgfxGlue.h"
#include "osystemAL.h"
#include <SDL.h>
#include <SDL_system.h>
#include <SDL_thread.h>
#include <SDL_mutex.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <android/log.h>

extern void detectGame(void);
extern void renderGameWindow(void);
extern "C" int FitdMain(int argc, char* argv[]);
int osystem_mouseLeft=0;
int osystem_mouseRight=0;

extern "C" { char homePath[512] = {}; }

static SDL_Window* engineWindow = nullptr;
static SDL_GLContext engineContext = nullptr;
static SDL_sem* startOfRender = nullptr;
static SDL_sem* endOfRender = nullptr;
static SDL_Thread* engineThread = nullptr;
static std::atomic<bool> engineExitRequested{false};
static std::atomic<bool> engineWorkerExited{false};
static std::atomic<int> engineWorkerResult{-1};
extern "C" void fitdAndroidRequestExit(void) {
    engineExitRequested.store(true);
}
bool bFirst = true;
static uint32_t engineTickTimestamp = 0;
static FILE* externalRuntimeLog=nullptr;
static void fitdRuntimeLogCallback(void*,int category,
                                   SDL_LogPriority priority,
                                   const char* message) {
    if(externalRuntimeLog) {
        fprintf(externalRuntimeLog,"%u [%d:%d] %s\n",
                SDL_GetTicks(),category,(int)priority,message?message:"");
        fflush(externalRuntimeLog);
    }
    __android_log_write(ANDROID_LOG_INFO,"FITD-REAL",message?message:"");
}
static void openRuntimeLog() {
    const char* root=SDL_AndroidGetExternalStoragePath();
    if(root && strlen(root)<440) {
        char path[512]={};
        snprintf(path,sizeof(path),"%s/fitd-test08-runtime.txt",root);
        externalRuntimeLog=fopen(path,"w");
        SDL_LogSetOutputFunction(fitdRuntimeLogCallback,nullptr);
        SDL_SetLogPriorities(SDL_LOG_PRIORITY_INFO);
        SDL_Log("FITD-RUNTIME: log_file=%s",path);
    }
}

void osystem_delay(int time) {
    SDL_Delay(time>0 ? static_cast<Uint32>(time) : 0);
}
void osystem_updateImage() {}

extern "C" void Sound_Quit(void) {}
void OPL_musicPlayer(void* context, Uint8* stream, int count) {
    musicUpdate(context,stream,count);
}
bool fileExists(const char* name) {
    if(!name || !name[0])return false;
    FILE* f=fopen(name,"rb");
    if(!f)return false;
    fclose(f);return true;
}

static int SDLCALL fitdEngineThread(void*) {
    SDL_Log("FITD-RUNTIME: REAL FitdMain worker started");
    if(SDL_GL_MakeCurrent(engineWindow,engineContext)!=0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "FITD-RUNTIME: GL make-current failed: %s",SDL_GetError());
        engineWorkerResult.store(27);
    } else {
        SDL_Log("FITD-RUNTIME: GL ES context ownership transferred to worker");
        engineWorkerResult.store(FitdMain(0,nullptr));
    }
    engineWorkerExited.store(true);
    engineExitRequested.store(true);
    SDL_SemPost(endOfRender); // release UI thread even after a startup error
    return engineWorkerResult.load();
}

void osystem_init() {
    // Do not initialize SDL from a second worker: main thread owns events.
    osystem_mouseLeft=0;
    osystem_mouseRight=0;
    osystem_initGL(640,480);
    osystemAL_init();
}

int FitdInit(int argc,char* argv[]) {
    (void)argc;(void)argv;
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK|
                SDL_INIT_GAMECONTROLLER|SDL_INIT_AUDIO)!=0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "FITD-RUNTIME: E_SDL_INIT %s",SDL_GetError());
        return 10;
    }
    openRuntimeLog();
    const char* storage=SDL_AndroidGetInternalStoragePath();
    if(!storage || strlen(storage)+2>=sizeof(homePath)) {
        SDL_Log("FITD-RUNTIME: E_PRIVATE_STORAGE");
        SDL_Quit();return 11;
    }
    snprintf(homePath,sizeof(homePath),"%s/",storage);
    if(chdir(storage)!=0 ||
       !fileExists("LISTBODY.PAK") ||
       !fileExists("LISTBOD2.PAK")) {
        SDL_Log("FITD-RUNTIME: E_REQUIRED_GAME_FILES");
        SDL_Quit();return 12;
    }
    SDL_Log("FITD-RUNTIME: assets detected in private storage");
    startOfRender=SDL_CreateSemaphore(0);
    endOfRender=SDL_CreateSemaphore(0);
    if(!startOfRender||!endOfRender) {
        SDL_Log("FITD-RUNTIME: E_SEMAPHORE %s",SDL_GetError());
        SDL_Quit();return 13;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    engineWindow=SDL_CreateWindow("Alone in the Dark",SDL_WINDOWPOS_CENTERED,
                      SDL_WINDOWPOS_CENTERED,640,480,
                      SDL_WINDOW_OPENGL|SDL_WINDOW_FULLSCREEN);
    if(!engineWindow) {
        SDL_Log("FITD-RUNTIME: E_WINDOW %s",SDL_GetError());
        SDL_Quit();return 14;
    }
    engineContext=SDL_GL_CreateContext(engineWindow);
    if(!engineContext) {
        SDL_Log("FITD-RUNTIME: E_GLES2 %s",SDL_GetError());
        SDL_DestroyWindow(engineWindow);SDL_Quit();return 15;
    }
    if(SDL_GL_MakeCurrent(engineWindow,nullptr)!=0) {
        SDL_Log("FITD-RUNTIME: E_DETACH_GLES2 %s",SDL_GetError());
        SDL_GL_DeleteContext(engineContext);
        SDL_DestroyWindow(engineWindow);SDL_Quit();return 16;
    }
    gWindowBGFX=engineWindow;
    // The real upstream detection routine initializes game type and CVars.
    detectGame();
    SDL_Log("FITD-RUNTIME: game detected, creating real FitdMain thread");
    osystem_init();
    engineThread=SDL_CreateThread(fitdEngineThread,"FITD-Real-Game",nullptr);
    if(!engineThread) {
        SDL_Log("FITD-RUNTIME: E_THREAD %s",SDL_GetError());
        SDL_GL_DeleteContext(engineContext);
        SDL_DestroyWindow(engineWindow);SDL_Quit();return 17;
    }
    engineTickTimestamp=SDL_GetTicks();
    // Main/UI thread receives input; engine worker waits for render ticks.
    while(!engineExitRequested.load()) {
        readKeyboard();
        if(engineExitRequested.load())break;
        SDL_SemPost(startOfRender);
        int code=SDL_SemWaitTimeout(endOfRender,30000);
        if(code==SDL_MUTEX_TIMEDOUT) {
            SDL_Log("FITD-RUNTIME: E_GAME_THREAD_TIMEOUT");
            engineExitRequested.store(true);
            break;
        }
        if(code!=0) {
            SDL_Log("FITD-RUNTIME: E_RENDER_SYNC %s",SDL_GetError());
            engineExitRequested.store(true);
            break;
        }
        Uint32 elapsed=SDL_GetTicks()-engineTickTimestamp;
        // Original FITD works with 40ms fixed simulation steps.
        // Never advance simulation multiple steps just because one frame is late.
        // Output frame pacing can later be interpolated separately to 60 Hz.
        if(elapsed<40u)SDL_Delay(40u-elapsed);
        engineTickTimestamp=SDL_GetTicks();
    }
    if(!engineWorkerExited.load()) {
        // The original engine has no cooperative cancellation yet.
        // Don't delete GL resources while its worker may be using them.
        SDL_Log("FITD-RUNTIME: engine worker did not exit; preserving GPU context");
        return 18;
    }
    int result=engineWorkerResult.load();
    SDL_WaitThread(engineThread,nullptr);
    SDL_GL_DeleteContext(engineContext);
    SDL_DestroyWindow(engineWindow);
    SDL_DestroySemaphore(startOfRender);
    SDL_DestroySemaphore(endOfRender);
    SDL_Log("FITD-RUNTIME: real engine exited rc=%d",result);
    if(externalRuntimeLog){fclose(externalRuntimeLog);externalRuntimeLog=nullptr;}
    SDL_Quit();
    return result;
}

u32 osystem_startOfFrame() {
    if(SDL_SemWaitTimeout(startOfRender,30000)!=0) {
        SDL_Log("FITD-RUNTIME: E_WAIT_FOR_FRAME");
        engineExitRequested.store(true);
        return 0;
    }
    StartFrame();
    osystem_startFrame();
    bFirst=false;
    return 1; // one logic step; no automatic frame skipping
}
void osystem_endOfFrame() {
    osystem_flushPendingPrimitives();
    osystem_drawUILayer();
    renderGameWindow();
    EndFrame();
    SDL_SemPost(endOfRender);
}
