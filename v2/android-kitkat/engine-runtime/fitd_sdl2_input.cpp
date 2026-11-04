/*
 * Genuine FitdLib/input.cpp port to SDL2 + physical Bluetooth pad on API19.
 * Exposes the ORIGINAL engine globals JoyD, Click and key via vars.h.
 * Builds only on isolated Android runtime, never on the stable R36S release.
 */
#include "common.h"
#include "fitd_gamepad_select_sdl2.h"
#include "fitd_gameplay_input_sdl2.h"
#include <SDL.h>

extern "C" void fitdAndroidRequestExit();
static SDL_GameController* fitdController=nullptr;
static SDL_JoystickID fitdControllerId=-1;
static Uint32 lastGamepadScan=0;
static bool wantsNewController=true;

static void closeController(){
    if(fitdController)SDL_GameControllerClose(fitdController);
    fitdController=nullptr;
    fitdControllerId=-1;
    wantsNewController=true;
}
static void findController(){
    if(fitdController)return;
    Uint32 tick=SDL_GetTicks();
    if(!wantsNewController && tick-lastGamepadScan<1700u)return;
    wantsNewController=false;
    lastGamepadScan=tick;
    const fitd_android::GamepadChoice candidate =
        fitd_android::selectBestPhysicalGamepad();
    if(candidate.index<0||!SDL_IsGameController(candidate.index))return;
    fitdController=SDL_GameControllerOpen(candidate.index);
    if(fitdController){
        fitdControllerId=SDL_JoystickInstanceID(
            SDL_GameControllerGetJoystick(fitdController));
        SDL_Log("FITD-RUNTIME: physically selected gamepad index=%d id=%d name=%s",
                candidate.index,(int)fitdControllerId,
                SDL_GameControllerName(fitdController));
    }
}
void readKeyboard(void){
    SDL_Event event;
    while(SDL_PollEvent(&event)){
        switch(event.type){
            case SDL_QUIT:
                fitdAndroidRequestExit();
                break;
            case SDL_KEYDOWN:
                if(event.key.keysym.sym==SDLK_AC_BACK &&
                   event.key.repeat==0)
                    fitdAndroidRequestExit();
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
            default: break;
        }
    }
    findController();
    // Unmodified engine expects these three mutable global fields. Update
    // before signaling the render thread's next 40ms logic step.
    fitd_android::applyGameplayInput(fitdController,
                                     reinterpret_cast<unsigned char&>(JoyD),
                                     Click,key);
    debuggerVar_fastForward=false;
}
