/*
 * SDL2 Android JNI entrypoint for the REAL FITD gameplay program.
 *
 * The Java SDLActivity starts native SDL_main from libmain.so. This forwards
 * into the platform's FitdInit() which performs game detection and launches
 * the original FitdMain() worker (compiled from upstream FitdLib/main.cpp).
 * Keep this separate from the R36S Linux Fitd/fitd.cpp executable.
 */
#include <SDL.h>
extern int FitdInit(int argc,char* argv[]);
extern "C" __attribute__((visibility("default"))) int SDL_main(int argc,char* argv[]){
    return FitdInit(argc,argv);
}
