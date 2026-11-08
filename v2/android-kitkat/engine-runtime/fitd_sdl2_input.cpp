/*
 * FITD Android TV input bridge (Android 4.4+/API19).
 * Android TV remotes can appear as gpio_ir_recv / virtual-search and must
 * never be selected instead of a physical iPega PG-9068 controller.
 * SDL2 keyboard events remain valid as a fallback for TV remote D-pad/OK.
 */
#include "common.h"
#include "fitd_gamepad_select_sdl2.h"
#include "fitd_gameplay_input_sdl2.h"
#include <SDL.h>
#include "fitd_touch_layout.h"
#include <map>
#include <utility>
#include <atomic>
#include <cmath>

extern "C" void fitdAndroidRequestExit();
extern "C" void fitdAndroidOnForeground();
extern "C" void fitdAndroidSetLifecyclePaused(int paused);
extern "C" int fitdAndroidTouchEnabled();
extern "C" void fitdAndroidSetGamepadConnected(int connected);
extern SDL_Window* gWindowBGFX;
static std::map<SDL_FingerID,std::pair<float,float>> touchFingers;
// Running in the 1992 game is triggered by a quick double directional tap.
// A full deflection on the virtual analog synthesizes two discrete presses
// and maintains the second one until the thumb returns to neutral.
static Uint8 runDirection=0;
static Uint32 runStarted=0;
static Uint32 lastRunLog=0;
static Uint8 analogRunPulse(Uint8 held, float sx, float sy, Uint32 now) {
    const float magnitude=std::sqrt(sx*sx+sy*sy);
    Uint8 candidate=0;
    if(magnitude>=.88f){
        if(std::fabs(sy)>=std::fabs(sx)*1.15f)
            candidate=sy<0.f?1:2;
        else if(std::fabs(sx)>=std::fabs(sy)*1.15f)
            candidate=sx<0.f?4:8;
    }
    // Hysteresis prevents accidental retrigger from normal thumb jitter.
    if(runDirection && magnitude<.72f){
        runDirection=0;runStarted=0;
    }
    if(!runDirection && candidate){
        runDirection=candidate;runStarted=now?now:1;
        SDL_Log("FITD-RUN: virtual double-tap START direction=%u",candidate);
    }else if(runDirection && candidate && candidate!=runDirection){
        runDirection=candidate;runStarted=now?now:1;
    }
    if(!runDirection)return held;
    const Uint32 elapsed=now-runStarted;
    if(elapsed<55u)return held|runDirection; // first key down
    if(elapsed<105u)return held & ~runDirection; // key up between taps
    if(elapsed<135u && now-lastRunLog>100u){
        lastRunLog=now;
        SDL_Log("FITD-RUN: HOLD second press direction=%u elapsed_ms=%u",
                runDirection,elapsed);
    }
    return held|runDirection; // second press is held for continuous running
}
static SDL_FingerID stickFinger=-1;
static float currentStickX=0.f,currentStickY=0.f;
static std::atomic<float> publishedStickX{0.f},publishedStickY{0.f};
extern "C" float fitdAndroidTouchStickX(){return publishedStickX.load(std::memory_order_acquire);}
extern "C" float fitdAndroidTouchStickY(){return publishedStickY.load(std::memory_order_acquire);}
static void clearTouch(){
    runDirection=0;
    runStarted=0;
    touchFingers.clear();
    stickFinger=-1;
    currentStickX=currentStickY=0.f;
    publishedStickX.store(0.f,std::memory_order_release);
    publishedStickY.store(0.f,std::memory_order_release);
}
static void windowSize(int& width,int& height){
    width=640;height=480;
    if(gWindowBGFX)SDL_GetWindowSize(gWindowBGFX,&width,&height);
}

static SDL_GameController* fitdController=nullptr;
static SDL_Joystick* fitdRawJoystick=nullptr;
static SDL_JoystickID fitdControllerId=-1;
static Uint32 lastGamepadScan=0;
static bool wantsNewController=true;
static bool tvBackPressed=false;
static Uint32 exitComboSince=0;

static void closeController(){
    if(fitdController)SDL_GameControllerClose(fitdController);
    if(fitdRawJoystick)SDL_JoystickClose(fitdRawJoystick);
    fitdController=nullptr;
    fitdRawJoystick=nullptr;
    fitdControllerId=-1;
    wantsNewController=true;
    fitdAndroidSetGamepadConnected(0);
}
static void findController(){
    if(fitdController || fitdRawJoystick)return;
    const Uint32 tick=SDL_GetTicks();
    if(!wantsNewController && tick-lastGamepadScan<1700u)return;
    wantsNewController=false;
    lastGamepadScan=tick;
    const fitd_android::GamepadChoice candidate=fitd_android::selectBestPhysicalGamepad();
    if(candidate.index<0)return;
    if(SDL_IsGameController(candidate.index)){
        fitdController=SDL_GameControllerOpen(candidate.index);
        if(fitdController){
            fitdControllerId=SDL_JoystickInstanceID(
                SDL_GameControllerGetJoystick(fitdController));
            SDL_Log("FITD-TV-INPUT: physical gamepad index=%d id=%d name=%s score=%d",
                candidate.index,(int)fitdControllerId,
                SDL_GameControllerName(fitdController),candidate.score);
        }
    } else {
        // Some Android TV firmware exposes the Bluetooth pad as a plain
        // joystick without a gamecontroller mapping.
        fitdRawJoystick=SDL_JoystickOpen(candidate.index);
        if(fitdRawJoystick){
            fitdControllerId=SDL_JoystickInstanceID(fitdRawJoystick);
            SDL_Log("FITD-TV-INPUT: raw Bluetooth joystick index=%d id=%d name=%s axes=%d buttons=%d hats=%d",
                candidate.index,(int)fitdControllerId,
                SDL_JoystickName(fitdRawJoystick),
                SDL_JoystickNumAxes(fitdRawJoystick),
                SDL_JoystickNumButtons(fitdRawJoystick),
                SDL_JoystickNumHats(fitdRawJoystick));
        }
    }
}
static bool rawButton(int idx){
    return fitdRawJoystick && idx<SDL_JoystickNumButtons(fitdRawJoystick)
        && SDL_JoystickGetButton(fitdRawJoystick,idx)!=0;
}
static void readRawJoystick(fitd_android::FitdInputState& state) {
    if(!fitdRawJoystick || !SDL_JoystickGetAttached(fitdRawJoystick))return;
    const Sint16 x=SDL_JoystickNumAxes(fitdRawJoystick)>0?
        SDL_JoystickGetAxis(fitdRawJoystick,0):0;
    const Sint16 y=SDL_JoystickNumAxes(fitdRawJoystick)>1?
        SDL_JoystickGetAxis(fitdRawJoystick,1):0;
    const Uint8 hat=SDL_JoystickNumHats(fitdRawJoystick)>0?
        SDL_JoystickGetHat(fitdRawJoystick,0):SDL_HAT_CENTERED;
    if(y<-12000 || (hat&SDL_HAT_UP))state.joyD|=1;
    if(y>12000  || (hat&SDL_HAT_DOWN))state.joyD|=2;
    if(x<-12000 || (hat&SDL_HAT_LEFT))state.joyD|=4;
    if(x>12000  || (hat&SDL_HAT_RIGHT))state.joyD|=8;
    if(rawButton(0)||rawButton(5))state.click=1;
    if(rawButton(3))state.key=0x1C;
    if(rawButton(1)||rawButton(7))state.key=0x1B;
}
void readKeyboard(void){
    SDL_Event event;
    while(SDL_PollEvent(&event)){
        switch(event.type){
            case SDL_APP_WILLENTERBACKGROUND:
            case SDL_APP_DIDENTERBACKGROUND:
                fitdAndroidSetLifecyclePaused(1);
                break;
            case SDL_APP_DIDENTERFOREGROUND:
                clearTouch();
                fitdAndroidOnForeground();
                fitdAndroidSetLifecyclePaused(0);
                wantsNewController=true;
                break;
            case SDL_FINGERDOWN:
                if(fitdAndroidTouchEnabled()){
                    int width,height;
                    windowSize(width,height);
                    const auto id=event.tfinger.fingerId;
                    if(stickFinger==-1 &&
                       fitd_touch::isStickStart(width,height,
                                    event.tfinger.x,event.tfinger.y)){
                        stickFinger=id;
                        currentStickX=event.tfinger.x;
                        currentStickY=event.tfinger.y;
                    }else{
                        touchFingers[id]=
                            std::make_pair(event.tfinger.x,event.tfinger.y);
                    }
                }
                break;
            case SDL_FINGERMOTION:
                if(fitdAndroidTouchEnabled()){
                    if(event.tfinger.fingerId==stickFinger){
                        currentStickX=event.tfinger.x;
                        currentStickY=event.tfinger.y;
                    }else if(touchFingers.count(event.tfinger.fingerId)){
                        touchFingers[event.tfinger.fingerId]=
                            std::make_pair(event.tfinger.x,event.tfinger.y);
                    }
                }
                break;
            case SDL_FINGERUP:
                if(event.tfinger.fingerId==stickFinger){
                    stickFinger=-1;
                    currentStickX=currentStickY=0.f;
                }
                touchFingers.erase(event.tfinger.fingerId);
                break;
            case SDL_WINDOWEVENT:
                if(event.window.event==SDL_WINDOWEVENT_SIZE_CHANGED ||
                   event.window.event==SDL_WINDOWEVENT_RESIZED)
                    clearTouch();
                break;
            case SDL_QUIT:
                clearTouch();
                fitdAndroidRequestExit();
                break;
            case SDL_KEYDOWN:
                // Android TV remote's BACK must open the game menu, not kill
                // the Activity or leave a stale SDL/EGL context behind.
                if(event.key.keysym.sym==SDLK_AC_BACK && event.key.repeat==0)
                    tvBackPressed=true;
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                if(event.cdevice.which==fitdControllerId)closeController();
                break;
            case SDL_JOYDEVICEREMOVED:
                if(event.jdevice.which==fitdControllerId)closeController();
                break;
            case SDL_CONTROLLERDEVICEADDED:
            case SDL_JOYDEVICEADDED:
                wantsNewController=true;
                break;
            default:break;
        }
    }
    // Re-evaluate hotplug and attachment every SDL pump. Closing a dropped
    // gamepad reactivates touch controls without requiring app restart.
    if(fitdController && !SDL_GameControllerGetAttached(fitdController))
        closeController();
    if(fitdRawJoystick && !SDL_JoystickGetAttached(fitdRawJoystick))
        closeController();
    findController();
    const bool padAttached=(fitdController && SDL_GameControllerGetAttached(fitdController)) ||
        (fitdRawJoystick && SDL_JoystickGetAttached(fitdRawJoystick));
    fitdAndroidSetGamepadConnected(padAttached?1:0);
    fitd_android::FitdInputState state=fitd_android::readGameplayInput(fitdController);
    readRawJoystick(state);
    if(!fitdAndroidTouchEnabled())clearTouch();
    if(fitdAndroidTouchEnabled()){
        int width,height;
        windowSize(width,height);
        float sx=0.f,sy=0.f;
        if(stickFinger!=-1){
            fitd_touch::stickAxes(width,height,currentStickX,currentStickY,sx,sy);
            // Digital FITD input repeats every engine tick while held.
            // Moving the analog knob toward UP continuously keeps UP pressed,
            // rather than emitting a one-off tap (important for running).
            const float threshold=.24f;
            if(sy < -threshold)state.joyD|=1;
            if(sy > threshold)state.joyD|=2;
            if(sx < -threshold)state.joyD|=4;
            if(sx > threshold)state.joyD|=8;
            state.joyD=analogRunPulse(state.joyD,sx,sy,SDL_GetTicks());
        }
        publishedStickX.store(sx,std::memory_order_release);
        publishedStickY.store(sy,std::memory_order_release);
        for(const auto& contact:touchFingers){
            const int btn=fitd_touch::buttonHit(width,height,
                                     contact.second.first,contact.second.second);
            switch(btn){
                case fitd_touch::ACTION:state.click=1;break;
                case fitd_touch::CONFIRM:state.key=0x1C;break;
                case fitd_touch::BACK:
                case fitd_touch::MENU:state.key=0x1B;break;
                default:break;
            }
        }
    }
    if(tvBackPressed){state.key=0x1B;tvBackPressed=false;}
    JoyD=state.joyD;
    Click=static_cast<char>(state.click);
    key=static_cast<char>(state.key);

    // Hold START+SELECT for two seconds to exit to Android TV launcher.
    const bool combo=(fitdController &&
        SDL_GameControllerGetButton(fitdController,SDL_CONTROLLER_BUTTON_START) &&
        SDL_GameControllerGetButton(fitdController,SDL_CONTROLLER_BUTTON_BACK)) ||
        (fitdRawJoystick && rawButton(6) && rawButton(7));
    if(combo) {
        if(!exitComboSince)exitComboSince=SDL_GetTicks();
        else if(SDL_GetTicks()-exitComboSince>2000u) {
            SDL_Log("FITD-TV-INPUT: START+SELECT hold -> clean exit requested");
            fitdAndroidRequestExit();
        }
    } else exitComboSince=0;
}
