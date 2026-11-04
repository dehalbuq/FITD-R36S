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
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <android/log.h>

// Presentation may run between 25 Hz logic ticks on the same GLES2 worker.
extern "C" bool fitdAndroidPresentCachedFrame();
extern "C" void fitdAndroidCaptureFrame();
static Uint64 lastPresentCounter=0;
static Uint32 renderedPresentations=0;
static Uint32 presentationWindowStart=0;
static void notePresentation() {
    lastPresentCounter=SDL_GetPerformanceCounter();
    ++renderedPresentations;
    const Uint32 now=SDL_GetTicks();
    if(!presentationWindowStart) presentationWindowStart=now;
    if(now-presentationWindowStart>=5000u) {
        SDL_Log("FITD-PERF: presentation_fps=%.1f (25 Hz logic, target <=60 Hz)",
            (renderedPresentations*1000.0)/(now-presentationWindowStart));
        renderedPresentations=0; presentationWindowStart=now;
    }
}
static bool presentationDue() {
    const Uint64 freq=SDL_GetPerformanceFrequency();
    const Uint64 now=SDL_GetPerformanceCounter();
    return !lastPresentCounter || (now-lastPresentCounter)>=freq/60u;
}

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
extern "C" void fitdAndroidQuitNow(void){
    SDL_Log("FITD-RUNTIME: USER_EXIT selected; terminating app");
    fflush(nullptr);
    // Explicit user-initiated abandon while engine is in its own SDL worker.
    // The original FitdMain is not cancellable; never destroy the EGL context
    // while its worker is running. Android can restart the app normally.
    std::_Exit(0);
}
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
        snprintf(path,sizeof(path),"%s/fitd-test13-runtime.txt",root);
        externalRuntimeLog=fopen(path,"w");
        if(externalRuntimeLog){
            // Preserve upstream printf()/fprintf(stderr) checkpoints in TXT
            // as well as SDL_Log, even across the native engine worker thread.
            fflush(stdout);fflush(stderr);
            dup2(fileno(externalRuntimeLog),STDOUT_FILENO);
            dup2(fileno(externalRuntimeLog),STDERR_FILENO);
            setvbuf(stdout,nullptr,_IONBF,0);
            setvbuf(stderr,nullptr,_IONBF,0);
        }
        SDL_LogSetOutputFunction(fitdRuntimeLogCallback,nullptr);
        SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);
        SDL_Log("FITD-RUNTIME: log_file=%s",path);
    }
}

void osystem_delay(int time) {
    SDL_Delay(time>0 ? static_cast<Uint32>(time) : 0);
}
void osystem_updateImage() {}

// Sound_Quit is now implemented by the native SDL2 audio backend.
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
        // The 60 Hz presentation cap is enforced by a monotonic clock, not
        // by requesting extra simulation ticks. Disable vsync where possible;
        // the physical display/driver can still impose its refresh rate.
        const int swapStatus=SDL_GL_SetSwapInterval(0);
        const int effectiveInterval=SDL_GL_GetSwapInterval();
        SDL_Log("FITD-GPU: requested_vsync=OFF set_result=%d effective_swap_interval=%d error=%s",
                swapStatus,effectiveInterval,swapStatus==0?"none":SDL_GetError());
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
    // Android GLES is GPU-backed by EGL; request a hardware accelerated visual.
    // Unlike SDL_Renderer, this app renders directly through GLES2.
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL,1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE,0);
    engineWindow=SDL_CreateWindow("Alone in the Dark",SDL_WINDOWPOS_CENTERED,
                      SDL_WINDOWPOS_CENTERED,640,480,
                      SDL_WINDOW_OPENGL|SDL_WINDOW_FULLSCREEN);
    if(!engineWindow) {
        SDL_Log("FITD-RUNTIME: E_WINDOW %s",SDL_GetError());
        SDL_Quit();return 14;
    }
    engineContext=SDL_GL_CreateContext(engineWindow);
    if(!engineContext) {
        SDL_Log("FITD-GPU: accelerated visual unavailable; retrying GLES2 default EGL config: %s",SDL_GetError());
        SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL,0);
        engineContext=SDL_GL_CreateContext(engineWindow);
    }
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
    // IMPORTANT: the previous implementation posted the NEXT simulation
    // semaphore BEFORE its 40ms sleep. The engine consumed it instantly, so
    // there was never any interval in which it could present cached frames.
    // Wait BEFORE posting each tick: the worker now owns GLES and presents
    // completed frames at up to 60 Hz while waiting for the next game tick.
    constexpr Uint32 logicMs=40;
    Uint32 nextLogic=SDL_GetTicks();
    SDL_Log("FITD-PERF: independent 25Hz simulation and 60Hz presentation active");
    while(!engineExitRequested.load()) {
        readKeyboard();
        if(engineExitRequested.load())break;
        const Uint32 now=SDL_GetTicks();
        if(static_cast<Sint32>(now-nextLogic)<0) {
            SDL_Delay(std::min<Uint32>(2, nextLogic-now));
            continue;
        }
        // At most one pending game tick; never enqueue catch-up bursts.
        SDL_SemPost(startOfRender);
        const int code=SDL_SemWaitTimeout(endOfRender,30000);
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
        nextLogic+=logicMs;
        // If frame work overran a tick, do not try to catch up by running
        // several gameplay loops at once. Recover the stable fixed cadence.
        const Uint32 after=SDL_GetTicks();
        if(static_cast<Sint32>(after-nextLogic)>=0)
            nextLogic=after+logicMs;
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
    // Engine worker owns GLES. Re-present its last complete scene up to
    // 60 Hz while waiting for the next 40 ms GAME tick from the SDL UI thread.
    // A repeated frame is not an additional simulation update.
    const Uint32 waitStarted=SDL_GetTicks();
    static uint32_t idleAttempts=0,idleSuccess=0,idleNoCache=0;
    static Uint32 lastIdleReport=0;
    for(;;) {
        const int waitResult=SDL_SemWaitTimeout(startOfRender,2);
        if(waitResult==0) {
            const Uint32 now=SDL_GetTicks();
            if(!lastIdleReport)lastIdleReport=now;
            if(now-lastIdleReport>=5000u) {
                SDL_Log("FITD-PERF: idle_present attempts=%u success=%u no_cache=%u last_wait_ms=%u",
                        idleAttempts,idleSuccess,idleNoCache,now-waitStarted);
                idleAttempts=idleSuccess=idleNoCache=0;
                lastIdleReport=now;
            }
            break;
        }
        if(waitResult!=SDL_MUTEX_TIMEDOUT || SDL_GetTicks()-waitStarted>30000u) {
            SDL_Log("FITD-RUNTIME: E_WAIT_FOR_FRAME %s",SDL_GetError());
            engineExitRequested.store(true);
            return 1;
        }
        if(engineExitRequested.load()) return 1;
        if(presentationDue()) {
            ++idleAttempts;
            if(fitdAndroidPresentCachedFrame()) {
                ++idleSuccess;
                notePresentation();
            } else {
                ++idleNoCache;
            }
        }
    }
    StartFrame();
    osystem_startFrame();
    bFirst=false;
    // One 40ms simulation step, regardless of 60Hz presentation frequency.
    // This prevents physics, actor animation and sound events from running at
    // 2.4x speed when interpolation is enabled.
    static Uint32 lastClock=0,sampled=0;
    const Uint32 now=SDL_GetTicks();
    const Uint32 elapsed=lastClock?now-lastClock:40;
    lastClock=now;
    if(++sampled%100u==0u)
        SDL_Log("FITD-PERF: logic_frames=%u recent_step_ms=%u tick=40",sampled,elapsed);
    return 1u;
}
void osystem_endOfFrame() {
    osystem_flushPendingPrimitives();
    osystem_drawUILayer();
    renderGameWindow();
    fitdAndroidCaptureFrame(); // GPU-side copy, no glReadPixels CPU stall
    // Show the previous frame at t=0, then interpolate toward the just
    // captured game image at up to 60Hz during the next 40ms logic interval.
    // On camera cuts the renderer shows the new frame immediately.
    while(!presentationDue() && !engineExitRequested.load())
        SDL_Delay(1);
    if(!fitdAndroidPresentCachedFrame())EndFrame();
    notePresentation();
    SDL_SemPost(endOfRender);
}
