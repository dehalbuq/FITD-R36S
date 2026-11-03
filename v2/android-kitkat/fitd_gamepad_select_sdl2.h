// Physical SDL2 gamepad selector for Android 4.4+ / API19 / armeabi-v7a.
// Affected firmware: Chanuo A1, Mediatek MT6735, Android API27.
// The Android OS enumerates its IR receiver and virtual-search inputs as SDL
// GameControllers BEFORE a real PG-9068, so opening joystick #0 is incorrect.
// Header-only to reuse when integrating the real FITD gameplay engine.
#pragma once
#include <SDL.h>
#include <cstring>

namespace fitd_android {

struct GamepadChoice {
    int index;           // SDL enumeration index (recalculate after hotplug)
    int score;           // heuristic quality, higher = physical gamepad
    SDL_JoystickID id;   // stable only while device is connected
    GamepadChoice() : index(-1), score(-1), id(-1) {}
};

inline bool isNonGameInput(const char* name) {
    if (!name) return true;
    // Android system/remote inputs that SDL may mark as GameController due to
    // key mappings; do not let these steal the first physical pad slot.
    return SDL_strcasecmp(name, "gpio_ir_recv") == 0 ||
           SDL_strcasecmp(name, "virtual-search") == 0 ||
           SDL_strcasecmp(name, "Android Accelerometer") == 0 ||
           SDL_strcasecmp(name, "Lm_Misc") == 0 ||
           SDL_strcasecmp(name, "ACCDET") == 0 ||
           SDL_strcasecmp(name, "mtk-kpd") == 0;
}

inline GamepadChoice selectBestPhysicalGamepad() {
    GamepadChoice best;
    const int count = SDL_NumJoysticks();
    for (int i = 0; i < count; ++i) {
        const char* name = SDL_JoystickNameForIndex(i);
        if (isNonGameInput(name)) continue;
        SDL_Joystick* joystick = SDL_JoystickOpen(i);
        if (!joystick) continue;
        const int axes = SDL_JoystickNumAxes(joystick);
        const int buttons = SDL_JoystickNumButtons(joystick);
        const int hats = SDL_JoystickNumHats(joystick);
        const SDL_JoystickID id = SDL_JoystickInstanceID(joystick);
        SDL_JoystickClose(joystick);
        if (buttons < 2 && hats < 1) continue;
        // Physical pads usually have sticks and multiple buttons. Crucially,
        // never assume SDL_IsGameController alone means "physical gamepad".
        int score = 0;
        if (axes >= 2) score += 100;
        if (axes >= 4) score += 20;
        if (buttons >= 4) score += 50;
        if (buttons >= 8) score += 10;
        if (hats > 0) score += 10;
        if (SDL_IsGameController(i)) score += 20;
        if (SDL_JoystickGetDeviceVendor(i) != 0 &&
            SDL_JoystickGetDeviceProduct(i) != 0) score += 10;
        if (score > best.score) {
            best.index = i;
            best.id = id;
            best.score = score;
        }
    }
    return best;
}

} // namespace fitd_android
