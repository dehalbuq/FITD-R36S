// Test 05, original FITD PAK index reader on files imported by the user.
// Real FITD PAK_getNumFiles() (FitdLib/pak.cpp) operates on app-private data.
// The wrapper validates headers BEFORE invoking the old parser on external data.
#include <SDL.h>
#include <SDL_system.h>
#include "pak.h"
#include <cstdio>
#include <cstring>
#include <cstdint>

extern "C" char homePath[512];

namespace {
struct PakCheck { const char* basename; bool required; };
const PakCheck kPaks[] = {
    {"LISTBOD2",true}, {"LISTBODY",true},
    {"LISTANIM",false}, {"LISTTRAK",false},
    {"LISTLIFE",false}, {"ETAGE00",false},
    {"ETAGE01",false}, {"ETAGE02",false}
};
bool prevalidatePak(const char* basename, long* resultSize) {
    char path[600] = {};
    if (snprintf(path,sizeof(path),"%s%s.PAK",homePath,basename) >= (int)sizeof(path))
        return false;
    FILE* f=fopen(path,"rb");
    if (!f) return false;
    bool valid=false;
    if (fseek(f,0,SEEK_END)==0) {
        long sz=ftell(f);
        if (sz>=12 && sz<=128L*1024L*1024L && fseek(f,4,SEEK_SET)==0) {
            unsigned char bytes[4];
            if (fread(bytes,1,4,f)==4) {
                uint32_t indexEnd=(uint32_t)bytes[0] |
                    ((uint32_t)bytes[1]<<8) |
                    ((uint32_t)bytes[2]<<16) |
                    ((uint32_t)bytes[3]<<24);
                // Original PAK_getNumFiles reads this index end pointer.
                // Ensure it is in file and describes one or more entries.
                if (indexEnd>=12 && indexEnd <= (uint32_t)sz &&
                    indexEnd%4==0 && ((indexEnd/4)-2)<65536) {
                    valid=true;*resultSize=sz;
                } else {
                    SDL_Log("FITD-GAME-DATA: invalid PAK index %s offset=%u size=%ld",
                            basename,indexEnd,sz);
                }
            }
        }
    }
    fclose(f);
    return valid;
}
}
extern "C" int fitd_android_game_data_probe() {
    const char* directory=SDL_AndroidGetInternalStoragePath();
    if (!directory || std::strlen(directory)+2>=sizeof(homePath)) {
        SDL_Log("FITD-GAME-DATA: ERROR cannot find app-private directory");
        return -1;
    }
    std::snprintf(homePath,sizeof(homePath),"%s/",directory);
    int found=0;
    for (const PakCheck& item:kPaks) {
        long size=0;
        if (!prevalidatePak(item.basename,&size)) {
            if (item.required) {
                SDL_Log("FITD-GAME-DATA: missing or invalid required %s.PAK",
                        item.basename);
                return -2;
            }
            continue;
        }
        // Call original game engine routine rather than reimplementing it.
        unsigned int count=PAK_getNumFiles(item.basename);
        if (count==0 || count>65536) {
            SDL_Log("FITD-GAME-DATA: invalid count from real engine %s: %u",
                    item.basename,count);
            if (item.required) return -3;
            continue;
        }
        SDL_Log("FITD-GAME-DATA: validated %s.PAK (%ld bytes, %u entries), "
                "via original FITD PAK_getNumFiles()",item.basename,size,count);
        ++found;
    }
    if (found<2) return -4;
    SDL_Log("FITD-GAME-DATA: PASS. Original game data indexed in Android "
            "app-private storage. Gameplay renderer not yet integrated.");
    return found;
}
