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
#include <SDL_opengles2.h>
#include <SDL_thread.h>
#include <SDL_mutex.h>
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <android/log.h>

// SDL2's Android pause event can block SDL_PollEvent before the game thread
// releases its EGL context. Keep the event pump alive and coordinate explicitly.
extern "C" void fitdAndroidSetLifecyclePaused(int paused);
extern "C" bool fitdAndroidRecoverGraphicsIfNeeded();


// Presentation may run between 25 Hz logic ticks on the same GLES2 worker.
extern "C" bool fitdAndroidPresentCachedFrame();
extern "C" void fitdAndroidCaptureFrame();
// High-resolution presentation clock, independent of the original 40ms
// simulation clock. Test15 rendered once per logic frame and waited for an
// additional optional swap; those waits quantized output to ~51 FPS.
static Uint64 presentDeadline=0;
static Uint64 presentPeriod=0;
static Uint32 renderedPresentations=0;
static Uint32 presentationWindowStart=0;
static Uint32 cachedPresentationAttempts=0,cachedPresentationSuccesses=0;
static std::atomic<Uint32> realLogicCounter{0};
static Uint32 lastReportedLogicCounter=0;
static Uint32 presentationOverruns=0;
static void notePresentation() {
    ++renderedPresentations;
    const Uint32 now=SDL_GetTicks();
    if(!presentationWindowStart)presentationWindowStart=now;
    if(now-presentationWindowStart>=5000u) {
        const Uint32 logicCount=realLogicCounter.load(std::memory_order_relaxed);
        const double windowSeconds=(now-presentationWindowStart)/1000.0;
        SDL_Log("FITD-PERF: presentation_fps=%.1f engine_loop_fps=%.1f target_logic_hz=60 target_present_hz=60 gpu_attempts=%u gpu_success=%u overruns=%u",
            renderedPresentations/windowSeconds,
            (logicCount-lastReportedLogicCounter)/windowSeconds,
            cachedPresentationAttempts,cachedPresentationSuccesses,presentationOverruns);
        lastReportedLogicCounter=logicCount;
        renderedPresentations=0;
        cachedPresentationAttempts=cachedPresentationSuccesses=presentationOverruns=0;
        presentationWindowStart=now;
    }
}
static void presentOnFixedClock() {
    const Uint64 freq=SDL_GetPerformanceFrequency();
    if(!freq)return;
    if(!presentPeriod)presentPeriod=std::max<Uint64>(1,freq/60u);
    const Uint64 now=SDL_GetPerformanceCounter();
    if(!presentDeadline)presentDeadline=now;
    if(now<presentDeadline)return;
    ++cachedPresentationAttempts;
    if(fitdAndroidPresentCachedFrame()) {
        ++cachedPresentationSuccesses;
        notePresentation();
    }
    // Preserve fractional phase, but discard stale deadlines after a stall.
    if(now-presentDeadline>presentPeriod*2u) {
        ++presentationOverruns;
        presentDeadline=now+presentPeriod;
    } else {
        presentDeadline+=presentPeriod;
    }
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
struct FitdAndroidStopEngine {};
static std::atomic<bool> engineWorkerExited{false};
static std::atomic<int> engineWorkerResult{-1};
static std::atomic<bool> touchHardwareAvailable{false};
extern "C" void fitdAndroidSetTouchHardware(int enabled) {
    touchHardwareAvailable.store(enabled!=0,std::memory_order_release);
    SDL_Log("FITD-TOUCH: control overlay %s",enabled?"ENABLED":"DISABLED");
}
extern "C" int fitdAndroidTouchEnabled(){
    return touchHardwareAvailable.load(std::memory_order_acquire)?1:0;
}
static std::atomic<bool> appInBackground{false};
static std::atomic<bool> workerContextDetached{false};
static std::atomic<bool> pendingGpuReset{false};

static int SDLCALL fitdLifecycleEventFilter(void*,SDL_Event* event) {
    if(event->type==SDL_APP_WILLENTERBACKGROUND) {
        appInBackground.store(true,std::memory_order_release);
        SDL_Log("FITD-LIFECYCLE: WILL_ENTER_BACKGROUND worker_detached=%d",
                workerContextDetached.load()?1:0);
    } else if(event->type==SDL_APP_DIDENTERFOREGROUND) {
        SDL_Log("FITD-LIFECYCLE: DID_ENTER_FOREGROUND (await main event pump)");
    } else if(event->type==SDL_RENDER_DEVICE_RESET) {
        pendingGpuReset.store(true);
        SDL_Log("FITD-LIFECYCLE: SDL_RENDER_DEVICE_RESET received");
    }
    return 1;
}

extern "C" void fitdAndroidOnForeground() {
    // SDL2 may temporarily make the restored context current on SDL_main.
    // The worker must be its sole long-lived EGL owner.
    if(workerContextDetached.load(std::memory_order_acquire)) {
        if(SDL_GL_MakeCurrent(engineWindow,nullptr)!=0)
            SDL_Log("FITD-LIFECYCLE: main GL detach: %s",SDL_GetError());
    }
    appInBackground.store(false,std::memory_order_release);
    SDL_Log("FITD-LIFECYCLE: foreground resume requested");
}

static void fitdWaitIfBackground() {
    if(!appInBackground.load(std::memory_order_acquire))return;
    if(!workerContextDetached.load(std::memory_order_acquire)) {
        SDL_Log("FITD-LIFECYCLE: worker entering background; releasing EGL");
        glFlush();
        if(SDL_GL_MakeCurrent(engineWindow,nullptr)!=0)
            SDL_Log("FITD-LIFECYCLE: worker GL detach: %s",SDL_GetError());
        workerContextDetached.store(true,std::memory_order_release);
    }
    while(appInBackground.load(std::memory_order_acquire) &&
          !engineExitRequested.load())SDL_Delay(10);
    if(engineExitRequested.load())throw FitdAndroidStopEngine{};
    SDL_Log("FITD-LIFECYCLE: worker reacquiring EGL");
    if(SDL_GL_MakeCurrent(engineWindow,engineContext)!=0) {
        SDL_Log("FITD-LIFECYCLE: EGL_REATTACH_FAILED %s",SDL_GetError());
        engineExitRequested.store(true);
        throw FitdAndroidStopEngine{};
    }
    workerContextDetached.store(false,std::memory_order_release);
    if(!fitdAndroidRecoverGraphicsIfNeeded()) {
        SDL_Log("FITD-LIFECYCLE: GPU_RECOVERY_FAILED");
        engineExitRequested.store(true);
        throw FitdAndroidStopEngine{};
    }
    if(pendingGpuReset.exchange(false))
        SDL_Log("FITD-LIFECYCLE: device reset event acknowledged");
    // Do not 'catch up' with an hour of frames after unlocking the screen.
    presentDeadline=0;
    presentPeriod=0;
    presentationWindowStart=0;
    renderedPresentations=0;
    lastReportedLogicCounter=realLogicCounter.load();
    SDL_Log("FITD-LIFECYCLE: RESUMED; EGL state checked");
}

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
        snprintf(path,sizeof(path),"%s/fitd-androidtv-test21-runtime.txt",root);
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
        try {
            engineWorkerResult.store(FitdMain(0,nullptr));
        } catch(const FitdAndroidStopEngine&) {
            SDL_Log("FITD-RUNTIME: engine worker cancelled cleanly at frame boundary");
            engineWorkerResult.store(0);
        }
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
    SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE,"0");
    SDL_SetHint(SDL_HINT_ORIENTATIONS,"LandscapeLeft LandscapeRight Portrait PortraitUpsideDown");
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK|
                SDL_INIT_GAMECONTROLLER|SDL_INIT_AUDIO)!=0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "FITD-RUNTIME: E_SDL_INIT %s",SDL_GetError());
        return 10;
    }
    openRuntimeLog();
    SDL_SetEventFilter(fitdLifecycleEventFilter,nullptr);
    SDL_Log("FITD-LIFECYCLE: nonblocking SDL2 Android pause handling enabled");
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
    // R36S-style native-engine experiment: raise actual PlayWorld iterations,
    // including model pose generation, from 25Hz to 60Hz. This is NOT just a
    // shader swap or a GPU frame-rate override. Each iteration receives one
    // engine tick and is run on the actual FitdMain worker. It deliberately
    // changes gameplay timing and must be validated for animation/script pace.
    constexpr Uint64 logicHz=60;
    const Uint64 frequency=SDL_GetPerformanceFrequency();
    const Uint64 period=std::max<Uint64>(1,frequency/logicHz);
    Uint64 nextLogic=SDL_GetPerformanceCounter();
    SDL_Log("FITD-ENGINE: REAL game loop UNLOCKED target=%uHz old=25Hz tick_ms=%.3f",
            (unsigned)logicHz,1000.0*(double)period/(double)frequency);
    while(!engineExitRequested.load()) {
        readKeyboard();
        if(engineExitRequested.load())break;
        if(appInBackground.load(std::memory_order_acquire)) {
            nextLogic=SDL_GetPerformanceCounter();
            SDL_Delay(10);
            continue;
        }
        const Uint64 now=SDL_GetPerformanceCounter();
        if(now<nextLogic) {
            if(nextLogic-now>frequency/500u)SDL_Delay(1);
            continue;
        }
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
        nextLogic+=period;
        const Uint64 after=SDL_GetPerformanceCounter();
        if(after>nextLogic+period)nextLogic=after+period;
    }
    if(!engineWorkerExited.load()) {
        SDL_SemPost(startOfRender);
        const Uint32 deadline=SDL_GetTicks()+1000u;
        while(!engineWorkerExited.load() &&
              static_cast<Sint32>(SDL_GetTicks()-deadline)<0)
            SDL_Delay(2);
        if(!engineWorkerExited.load()) {
            SDL_Log("FITD-RUNTIME: WARN engine worker still active on shutdown");
            return 18;
        }
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
    // The worker is the only EGL context owner. Present on the absolute
    // 60Hz clock during its idle intervals, not only at osystem_endOfFrame().
    // Do not hold the worker waiting for a 25Hz game tick before presenting.
    const Uint32 waitStarted=SDL_GetTicks();
    for(;;) {
        if(engineExitRequested.load())throw FitdAndroidStopEngine{};
        fitdWaitIfBackground();
        presentOnFixedClock();
        const int waitResult=SDL_SemWaitTimeout(startOfRender,1);
        if(waitResult==0)break;
        if(waitResult!=SDL_MUTEX_TIMEDOUT || SDL_GetTicks()-waitStarted>30000u) {
            SDL_Log("FITD-RUNTIME: E_WAIT_FOR_FRAME %s",SDL_GetError());
            engineExitRequested.store(true);
            return 1;
        }
    }
    StartFrame();
    osystem_startFrame();
    bFirst=false;
    // Experimental native-engine 60Hz update: the actual game, AI, actor
    // animation and geometry now execute at up to 60 frames each second.
    // This can change legacy gameplay pace. Keep Test17 installed as backup.
    static Uint32 lastClock=0,sampled=0;
    const Uint32 now=SDL_GetTicks();
    const Uint32 elapsed=lastClock?now-lastClock:40;
    lastClock=now;
    realLogicCounter.fetch_add(1,std::memory_order_relaxed);
    if(++sampled%120u==0u)
        SDL_Log("FITD-ENGINE: actual_update=%u recent_step_ms=%u tick=1 (native 60Hz experimental)",
                sampled,elapsed);
    return 1u;
}
void osystem_endOfFrame() {
    fitdWaitIfBackground();
    osystem_flushPendingPrimitives();
    osystem_drawUILayer();
    renderGameWindow();
    fitdAndroidCaptureFrame(); // swap GLES2 FBO attachments, no pixel copy
    if(engineExitRequested.load())throw FitdAndroidStopEngine{};
    // At 60Hz native simulation there may be no idle time between two
    // PlayWorld iterations. Present the freshly generated frame here as well,
    // otherwise the previous 25Hz idle-only path can starve the display.
    presentOnFixedClock();
    SDL_SemPost(endOfRender);
}
