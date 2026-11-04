/*
 * TEMPORARY SILENT audio backend for first REAL FitdMain GLES2 boot.
 * Isolated to experiment; SoLoud/OpenSL ES must replace this before a release.
 * Original music/sound assets and game timing are not changed.
 */
#include <SDL.h>
extern "C" void osystemAL_init(void) {
    SDL_Log("FITD-AUDIO: experimental silent backend; audio not initialized");
}
extern "C" void osystemAL_udpate(void) {}
void osystem_playSample(char*,int) {}
int osystem_playTrack(int) {return 0;}
