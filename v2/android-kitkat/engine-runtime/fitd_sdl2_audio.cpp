/*
 * Experimental SDL2 audio backend for the real FITD engine, Android API19.
 *
 * Music: original FM/OPL engine's musicUpdate() generates S16 mono/44100.
 * Sound effects: AITD1 resource bytes #32+ are unsigned 8-bit PCM. Each
 * voice is mixed at its own sample rate, with a 10-voice cap and copy of
 * the resource so no dangling pointers when upstream evicts a PAK cache.
 *
 * No SoLoud, SDL3, OpenAL or external content dependency.
 */
#include "common.h"
#include "osystemAL.h"
#include <SDL.h>
#include <vector>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <cstdio>

namespace {
static SDL_AudioDeviceID device=0;
static constexpr int RATE=44100;
struct Voice {
    std::vector<uint8_t> pcm;
    double position=0.;
    double sampleStep=1.;
};
static std::vector<Voice> voices;
static std::atomic<float> audioVolume{0.80f};
static std::atomic<bool> musicEnabledAndroid{true};
static std::atomic<bool> effectsEnabledAndroid{true};
static bool initialized=false;

static void SDLCALL audioCallback(void*,Uint8* stream,int bytes){
    memset(stream,0,bytes);
    const int n=bytes/(int)sizeof(int16_t);
    if(n<1||!initialized)return;
    // The original OPL emulator synthesizes the game's music at 44.1kHz.
    musicUpdate(nullptr,(uint8*)stream,n*(int)sizeof(int16_t));
    const bool playMusic=musicEnabledAndroid.load(std::memory_order_relaxed);
    const bool playEffects=effectsEnabledAndroid.load(std::memory_order_relaxed);
    int16_t* out=(int16_t*)stream;
    const float level=audioVolume.load(std::memory_order_relaxed);
    for(int i=0;i<n;i++){
        float sample=playMusic?(float)out[i]*level:0.f;
        for(Voice& v:voices){
            const size_t cursor=(size_t)v.position;
            if(cursor>=v.pcm.size())continue;
            const float sound=(float)((int)v.pcm[cursor]-128)*230.f;
            if(playEffects)sample+=sound*level;
            v.position+=v.sampleStep;
        }
        if(sample>32767.f)sample=32767.f;
        if(sample<-32768.f)sample=-32768.f;
        out[i]=(int16_t)sample;
    }
    voices.erase(std::remove_if(voices.begin(),voices.end(),
        [](const Voice& v){return (size_t)v.position>=v.pcm.size();}),
        voices.end());
}
}
extern "C" void osystemAL_init(void){
    if(device)return;
    SDL_AudioSpec wanted={},got={};
    wanted.freq=RATE;
    wanted.format=AUDIO_S16SYS;
    wanted.channels=1;
    wanted.samples=1024;
    wanted.callback=audioCallback;
    device=SDL_OpenAudioDevice(nullptr,0,&wanted,&got,0);
    if(!device){
        SDL_Log("FITD-AUDIO: Android SDL2 output unavailable: %s",SDL_GetError());
        return;
    }
    if(got.freq!=RATE||got.format!=AUDIO_S16SYS||got.channels!=1){
        SDL_Log("FITD-AUDIO: unsupported output format");
        SDL_CloseAudioDevice(device);device=0;
        return;
    }
    initialized=true;
    SDL_PauseAudioDevice(device,0);
    SDL_Log("FITD-AUDIO: SDL2 S16 44100 Hz mono; authentic OPL + PCM enabled");
}
extern "C" void osystemAL_udpate(void){
    // The software mixer is serviced by the SDL2 audio thread.
    // Do not call musicUpdate() twice from the graphics thread.
}
extern "C" void Sound_Quit(void){
    if(!device)return;
    SDL_PauseAudioDevice(device,1);
    SDL_CloseAudioDevice(device);
    device=0;initialized=false;
    voices.clear();
}
void osystem_playSample(char* sample,int size){
    if(!sample||size<34||!device || !effectsEnabledAndroid.load())return;
    if((uint8_t)sample[26]!=1)return;
    const unsigned hdr=(unsigned char)sample[26]|
        ((unsigned)(unsigned char)sample[27]<<8)|
        ((unsigned)(unsigned char)sample[28]<<16)|
        ((unsigned)(unsigned char)sample[29]<<24);
    const int len=(int)(hdr>>8)-2;
    const int available=size-32;
    if(len<=0||len>available||len>131072)return;
    const unsigned divider=(uint8_t)sample[30];
    const unsigned divisor=256u-divider;
    if(divisor==0)return;
    const unsigned sourceRate=1000000u/divisor;
    if(sourceRate<1000u||sourceRate>48000u)return;
    Voice newVoice;
    newVoice.pcm.assign((const uint8_t*)sample+32,
                        (const uint8_t*)sample+32+len);
    newVoice.sampleStep=(double)sourceRate/(double)RATE;
    SDL_LockAudioDevice(device);
    if(voices.size()>=10)voices.erase(voices.begin());
    voices.push_back(std::move(newVoice));
    SDL_UnlockAudioDevice(device);
}
int osystem_playTrack(int track){
    // Original AITD1 uses OPL+LISTSAMP. Optional CD-WAV tracks will be
    // handled separately; do not claim to play missing WAV content.
    (void)track;
    return 0;
}
void osystem_playAdlib(){}
void osystemAL_adlib_Update(){}

// Game menu toggles act on the actual OPL and PCM mixer, not just labels.
// Use atomics: callback executes on a separate SDL audio thread.
extern "C" void fitdAndroidSetAudioEnabled(int music,int effects){
    musicEnabledAndroid.store(music!=0,std::memory_order_relaxed);
    effectsEnabledAndroid.store(effects!=0,std::memory_order_relaxed);
    SDL_Log("FITD-OPTIONS: music=%s effects=%s",
            music?"ON":"OFF",effects?"ON":"OFF");
}
