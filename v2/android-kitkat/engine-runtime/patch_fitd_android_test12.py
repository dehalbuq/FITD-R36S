#!/usr/bin/env python3
"""Android-only source-pinned UI integration for FITD Test12.

Applies after patch_fitd_gameplay_api19.py and the R36S readable-font patch.
Never changes the stable R36S/Linux branch, original asset files, or upstream.
"""
from pathlib import Path
import sys

if len(sys.argv)!=2:
    raise SystemExit("usage: patch_fitd_android_test12.py UPSTREAM_SOURCE_DIR")
root=Path(sys.argv[1])
def one(path, old, new, label):
    f=root/path
    text=f.read_text()
    count=text.count(old)
    if count!=1:
        raise RuntimeError(f"TEST12 {label}: expected unique anchor in {path}, got {count}")
    f.write_text(text.replace(old,new,1))
    print("FITD-TEST12:",label)

# Reuse the original binary save format and atomic write safeguards; only
# introduce file slot numbers 0..4. SAVE0.ITD remains fully compatible.
p=root/"FitdLib/save.cpp"
s=p.read_text()
append=r"""
#ifdef FITD_R36S
int fitdAndroidSaveSlot(int slot) {
    if(slot<0 || slot>=5) {
        SDL_Log("FITD-SAVE: rejected invalid slot=%d",slot);
        return 0;
    }
    SDL_Log("FITD-SAVE: selected slot=%d",slot);
    return makeSaveFile(slot);
}
#endif
"""
assert s.count("int makeSave(int arg0)")==1
assert "fitdAndroidSaveSlot(" not in s
p.write_text(s+append)

# Menu selection keeps engine's original input and SDL2 worker/event threading.
# PrintFont targets the same uiLayer/CP850 R36S glyphs as existing messages.
p=root/"FitdLib/systemMenu.cpp"
s=p.read_text()
include='#include "common.h"'
assert s.count(include)==1
s=s.replace(include,include+'''
#ifdef FITD_R36S
#include <cstdio>
#include <cstring>
#include <SDL.h>
extern "C" void fitdAndroidSetAudioEnabled(int music,int effects);
extern "C" void fitdAndroidSetHighDetail(int high);
extern int fitdAndroidSaveSlot(int slot);
static bool fitdMusicOn=true;
static bool fitdEffectsOn=true;
static bool fitdDetailHigh=true;
static bool fitdPreferencesInitialized=false;
static void fitdInitPreferences(){
    if(fitdPreferencesInitialized)return;
    fitdPreferencesInitialized=true;
    musicEnabled=1;
    fitdAndroidSetAudioEnabled(1,1);
    fitdAndroidSetHighDetail(1);
}
static void fitdDrawState(int option,const char* status){
    const int y=WindowY1+((WindowY2-WindowY1)/2)-(NB_OPTIONS*SIZE_FONT)/2+
                option*SIZE_FONT;
    SetFont(PtrFont,15);
    PrintFont(226,y,logicalScreen,(u8*)status);
}
// This is an in-game slot picker, not an Android external file browser.
static int fitdChooseSlot(bool loading) {
    int selected=0;
    bool debounce=true;
    const int limit=5;
    while(true) {
        AffBigCadre(160,100,320,200);
        SetFont(PtrFont,15);
        const char* title=loading?"CARREGAR JOGO":"SALVAR JOGO";
        PrintFont(92,24,logicalScreen,(u8*)title);
        PrintFont(31,151,logicalScreen,(u8*)"A: CONFIRMAR  B: VOLTAR");
        for(int i=0;i<limit;i++) {
            char filename[20]={};
            std::snprintf(filename,sizeof(filename),"SAVE%d.ITD",i);
            bool present=false;
            FILE* f=std::fopen(filename,"rb");
            if(f) {
                if(std::fseek(f,0,SEEK_END)==0 && std::ftell(f)>4096)
                    present=true;
                std::fclose(f);
            }
            char line[90]={};
            std::snprintf(line,sizeof(line),"%c SLOT %d - %s",
                          i==selected?'>':' ',i+1,
                          present?"GRAVADO":"VAZIO");
            SetFont(PtrFont,i==selected?15:4);
            PrintFont(43,47+i*19,logicalScreen,(u8*)line);
        }
        osystem_CopyBlockPhys((unsigned char*)logicalScreen,0,0,320,200);
        osystem_startFrame();
        process_events();
        flushScreen();
        osystem_drawBackground();

        const int k=key,j=JoyD,c=Click;
        if(debounce) {
            if(!k && !j && !c)debounce=false;
        } else if(k==0x1B) {
            AntiRebond=1;
            SDL_Log("FITD-SAVE: slot picker cancelled");
            osystem_flip(nullptr);
            return -1;
        } else if(j==1 || j==2) {
            selected=(selected+(j==1?limit-1:1))%limit;
            debounce=true;
        } else if(k==0x1C || c) {
            char filename[20]={};
            std::snprintf(filename,sizeof(filename),"SAVE%d.ITD",selected);
            if(loading) {
                FILE* f=std::fopen(filename,"rb");
                bool available=false;
                if(f) {
                    if(std::fseek(f,0,SEEK_END)==0 && std::ftell(f)>4096)
                        available=true;
                    std::fclose(f);
                }
                if(!available) {
                    SDL_Log("FITD-SAVE: load slot %d empty; choose another",selected);
                    debounce=true;
                } else {
                    AntiRebond=1;
                    osystem_flip(nullptr);
                    return selected;
                }
            } else {
                AntiRebond=1;
                osystem_flip(nullptr);
                return selected;
            }
        }
        osystem_flip(nullptr);
    }
}
#endif
''',1)
# NB_OPTIONS/SIZE_FONT are preprocessor definitions after includes, so
# helper needs those defines placed ahead of its declaration.
start=s.index('#ifdef FITD_R36S\n#include <cstdio>')
stop=s.index('#endif\n',start)+len('#endif\n')
helper=s[start:stop]
s=s[:start]+s[stop:]
anchor='#define\tNB_OPTIONS\t7'
assert anchor in s
s=s.replace(anchor,anchor+'\n'+helper,1)

# Don't rely on incomplete original 41/42,43/44,49/50 English strings
# for menu state: display explicit ON/OFF and HIGH/LOW with CP850 UI font.
old='''    AffOption(3,41+musicEnabled,selectedStringNumber);
    AffOption(4,43/*+soundToggle*/,selectedStringNumber);
    AffOption(5,49/*+detailToggle*/,selectedStringNumber);
    AffOption(6,47,selectedStringNumber);'''
new='''#ifdef FITD_R36S
    fitdInitPreferences();
    AffOption(3,41,selectedStringNumber);
    AffOption(4,43,selectedStringNumber);
    AffOption(5,49,selectedStringNumber);
    fitdDrawState(3,fitdMusicOn?"ON":"OFF");
    fitdDrawState(4,fitdEffectsOn?"ON":"OFF");
    fitdDrawState(5,fitdDetailHigh?"ALTO":"BAIXO");
#else
    AffOption(3,41+musicEnabled,selectedStringNumber);
    AffOption(4,43/*+soundToggle*/,selectedStringNumber);
    AffOption(5,49/*+detailToggle*/,selectedStringNumber);
#endif
    AffOption(6,47,selectedStringNumber);'''
assert s.count(old)==1, "TEST12 options baseline mismatch"
s=s.replace(old,new,1)
old='''                    case 1: // save
#ifdef FITD_R36S
                        if(makeSave(45)) {
                            SDL_Log("FITD-SAVE: menu save succeeded; returning to game");
                            exitMenu=1;
                        } else {
                            SDL_Log("FITD-SAVE: menu save FAILED; check runtime log");
                        }
#else
                        makeSave(45);
#endif
                        break;
                    case 2: // load
                        if(restoreSave(46,1))
                        {
                            FlagInitView = 2;
                            RestoreTimerAnim();
                            //updateShaking();
                            return;
                        }
                        break;'''
new='''                    case 1: // save
#ifdef FITD_R36S
                    {
                        const int slot=fitdChooseSlot(false);
                        if(slot>=0) {
                            const int ok=fitdAndroidSaveSlot(slot);
                            SDL_Log("FITD-SAVE: menu saved slot=%d status=%d",slot,ok);
                            if(ok)exitMenu=1;
                        }
                        break;
                    }
#else
                        makeSave(45);
                        break;
#endif
                    case 2: // load
#ifdef FITD_R36S
                    {
                        const int slot=fitdChooseSlot(true);
                        if(slot>=0 && loadSave(slot)) {
                            SDL_Log("FITD-SAVE: menu restored slot=%d",slot);
                            FlagInitView=2;
                            RestoreTimerAnim();
                            return;
                        }
                        break;
                    }
#else
                        if(restoreSave(46,1)) {
                            FlagInitView=2; RestoreTimerAnim(); return;
                        }
                        break;
#endif
                    case 3: // music
#ifdef FITD_R36S
                        fitdMusicOn=!fitdMusicOn;
                        musicEnabled=fitdMusicOn?1:0;
                        fitdAndroidSetAudioEnabled(fitdMusicOn,fitdEffectsOn);
#endif
                        AntiRebond=1;
                        break;
                    case 4: // sound effects
#ifdef FITD_R36S
                        fitdEffectsOn=!fitdEffectsOn;
                        fitdAndroidSetAudioEnabled(fitdMusicOn,fitdEffectsOn);
#endif
                        AntiRebond=1;
                        break;
                    case 5: // graphical detail
#ifdef FITD_R36S
                        fitdDetailHigh=!fitdDetailHigh;
                        fitdAndroidSetHighDetail(fitdDetailHigh);
#endif
                        AntiRebond=1;
                        break;'''
assert s.count(old)==1,"TEST12 original patched system menu changed"
s=s.replace(old,new,1)
p.write_text(s)
print("FITD-TEST12: real audio/detail menu controls and 5 game save slots")
