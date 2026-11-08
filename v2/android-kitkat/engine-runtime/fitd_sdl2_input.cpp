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

extern "C" void fitdAndroidRequestExit();
extern "C" void fitdAndroidOnForeground();
extern "C" void fitdAndroidSetLifecyclePaused(int paused);

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
                fitdAndroidOnForeground();
                fitdAndroidSetLifecyclePaused(0);
                wantsNewController=true;
                break;
            case SDL_QUIT:
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
    findController();
    fitd_android::FitdInputState state=fitd_android::readGameplayInput(fitdController);
    readRawJoystick(state);
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
