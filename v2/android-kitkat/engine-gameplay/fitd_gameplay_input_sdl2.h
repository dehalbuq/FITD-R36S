// FITD gameplay input bridge for Android SDL2 / armeabi-v7a / API19+.
//
// Drop-in adapter matching the ORIGINAL FitdLib/vars.h legacy input globals:
//   JoyD: 1=up, 2=down, 4=left, 8=right
//   Click: 1=action (space / gamepad A)
//   key:   0x1C=confirm (Return / Y), 0x1B=menu (Escape / B/Start)
// No SDL3 dependency or game-specific UI code.
// Caller owns SDL_Init and SDL_GameControllerOpen/Close, to avoid grabbing the
// projector's IR receiver at SDL index 0 instead of the physical PG-9068.
#pragma once
#include <SDL.h>
#include <cstdint>

namespace fitd_android {

struct FitdInputState {
    uint8_t joyD;
    uint8_t click;
    uint8_t key;
    FitdInputState() : joyD(0), click(0), key(0) {}
};

inline FitdInputState readGameplayInput(
    SDL_GameController* controller,
    const uint8_t* pressedKeyboard = nullptr,
    Sint16 deadzone = 12000) {
    FitdInputState state;
    const Uint8* keys = pressedKeyboard ? pressedKeyboard : SDL_GetKeyboardState(nullptr);
    if (keys) {
        if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W])
            state.joyD |= 1;
        if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S])
            state.joyD |= 2;
        if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A])
            state.joyD |= 4;
        if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D])
            state.joyD |= 8;
        if (keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_Z])
            state.click = 1;
        if (keys[SDL_SCANCODE_RETURN])
            state.key = 0x1C;
        if (keys[SDL_SCANCODE_ESCAPE])
            state.key = 0x1B;
    }
    if (!controller || !SDL_GameControllerGetAttached(controller))
        return state;
    const Sint16 x = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
    const Sint16 y = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
    if (y < -deadzone || SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_DPAD_UP))
        state.joyD |= 1;
    if (y > deadzone || SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_DPAD_DOWN))
        state.joyD |= 2;
    if (x < -deadzone || SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_DPAD_LEFT))
        state.joyD |= 4;
    if (x > deadzone || SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
        state.joyD |= 8;
    if (SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_A) ||
        SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER))
        state.click = 1;
    if (SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_Y))
        state.key = 0x1C;
    if (SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_B) ||
        SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_START))
        state.key = 0x1B;
    return state;
}

inline void applyGameplayInput(
    SDL_GameController* controller,
    unsigned char& joyD, char& click, char& key) {
    const FitdInputState next = readGameplayInput(controller);
    joyD=next.joyD;
    click=static_cast<char>(next.click);
    key=static_cast<char>(next.key);
}
} // namespace fitd_android
