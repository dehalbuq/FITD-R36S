#!/usr/bin/env python3
"""Source-pinned Android 4.4/SDL2 compatibility edits for upstream FITD.

Only applies to temporary clone of yaz0r/FITD commit
543e22bc78b64405537ae545ecd382dcdf2c12b2. Never patches R36S
stable source or GitHub upstream in place.
"""
from pathlib import Path
import sys

if len(sys.argv)!=2:
    raise SystemExit("usage: patch_fitd_gameplay_api19.py /tmp/fitd-upstream")

root=Path(sys.argv[1])
def one(path, old, new, reason):
    f=root/path
    s=f.read_text()
    count=s.count(old)
    if count!=1:
        raise RuntimeError(f"{reason}: {path} found {count}, expected exactly 1")
    f.write_text(s.replace(old,new,1))
    print("FITD-API19:",reason)

# The upstream debug bounding-volume display escapes its FITD_DEBUGGER guard.
# It calls drawZv when the debug-only backgroundMode enum exists. This must
# be excluded from an actual non-debug release, not re-enable debug globally.
one("FitdLib/animAction.cpp",
    '''            if (backgroundMode == backgroundModeEnum_3D) {
                drawZv(rangeZv);
            }''',
    '''#ifdef FITD_DEBUGGER
            if (backgroundMode == backgroundModeEnum_3D) {
                drawZv(rangeZv);
            }
#endif''',
    "exclude debugger-only action collision volume overlay")

# Original hqr.cpp instantiates HQR_Reset<char> and HQR_Reset<sAnimation>,
# but forgets HQR_Reset<sBody>. The release game references that symbol;
# explicit instantiation is required by the C++ linker on Android ARMv7.
one("FitdLib/hqr.cpp",
    'template void HQR_Free(hqrEntryStruct<sBody>* hqrPtr);',
    'template void HQR_Free(hqrEntryStruct<sBody>* hqrPtr);\n'
    'template void HQR_Reset(hqrEntryStruct<sBody>* hqrPtr);',
    "instantiate original body resource cache reset for final link")


# Merge only source-pinned R36S gameplay fixes, not R36S Linux SDL3/GLES3
# patches, renderer diagnostics or the noisy debug audio crash reporter.
# This model-state fix (used on R36S) prevents wrongly dropping animated
# special objects from the game's active rendering and AI list.
one("FitdLib/main.cpp",
    'if(!(actorPtr->objectType & (AF_SPECIAL & AF_ANIMATED)))',
    'if(!(actorPtr->objectType & (AF_SPECIAL | AF_ANIMATED)))',
    "R36S animated-special actor flag fix")

# Original systemMenu.cpp displays option 6 ("ABANDONAR") but its switch
# handles only options 0,1,2. Android should actually leave when selected.
one("FitdLib/systemMenu.cpp",
    '                        break;\n\n                    }\n                }\n                else',
    '''                        break;
                    case 6: // abandon/exit application
#ifdef FITD_R36S
                        SDL_Log("FITD-ANDROID: menu ABANDONAR selected; exit requested");
                        fitdAndroidQuitNow();
#endif
                        exitMenu = 1;
                        break;

                    }
                }
                else''',
    "implement original menu option 6")
one("FitdLib/systemMenu.cpp",
    '#include "common.h"',
    '''#include "common.h"
#ifdef FITD_R36S
#include <SDL.h>
extern "C" void fitdAndroidQuitNow(void);
#endif''',
    "declare Android menu quit handler")

# Original save writes 300 records for AITD1, while LoadWorld may have a
# shorter vector. Reuse the proven R36S 300-record padding fix, assigning
# invalid object index to empty entries to avoid writing arbitrary data.
p=root/"FitdLib/save.cpp"
save=p.read_text()
target='''        oldNumMaxObj = maxObjects;
        maxObjects = 300; // fix for save engine..'''
assert save.count(target)==2, "expected exactly two AITD1 save/load blocks"
replace=target+'''
#ifdef FITD_R36S
        if (ListWorldObjets.size() < 300) {
            const size_t oldSize=ListWorldObjets.size();
            ListWorldObjets.resize(300);
            for(size_t n=oldSize;n<300;++n)
                ListWorldObjets[n].objIndex=-1;
            SDL_Log("FITD-ANDROID: padded save world objects %u -> 300",
                    (unsigned)oldSize);
        }
#endif'''
save=save.replace(target,replace)
save=save.replace('#include "common.h"',
    '#include "common.h"\n#ifdef FITD_R36S\n#include <SDL.h>\n#endif',1)
# An incomplete write should never replace a valid prior save.
old='''    sprintf(buffer,"SAVE%d.ITD",entry);

    fHandle = fopen(buffer,"wb+");'''
new='''    sprintf(buffer,"SAVE%d.ITD",entry);
#ifdef FITD_R36S
    char temporary[128];
    snprintf(temporary,sizeof(temporary),"SAVE%d.ITD.tmp",entry);
    fHandle = fopen(temporary,"wb+");
#else
    fHandle = fopen(buffer,"wb+");
#endif'''
assert save.count(old)==1,"save opener mismatch"
save=save.replace(old,new,1)
old='''    fclose(fHandle);

    return 1;
}

int makeSave(int arg0)'''
new='''#ifdef FITD_R36S
    const long savedBytes=ftell(fHandle);
    const int ioError=ferror(fHandle);
    const int closeError=fclose(fHandle);
    if(ioError||closeError||savedBytes<=0) {
        SDL_Log("FITD-ANDROID: save write failed slot=%d",entry);
        remove(temporary);
        return 0;
    }
    if(rename(temporary,buffer)!=0) {
        SDL_Log("FITD-ANDROID: atomic save rename failed slot=%d",entry);
        remove(temporary);
        return 0;
    }
    SDL_Log("FITD-ANDROID: save complete slot=%d bytes=%ld",entry,savedBytes);
#else
    fclose(fHandle);
#endif
    return 1;
}

int makeSave(int arg0)'''
assert save.count(old)==1,"save close mismatch"
save=save.replace(old,new,1)
p.write_text(save)
print("FITD-API19: restored R36S 300-entry save/load and atomic save protection")

# Prevent repeat door sounds (3/4), which the R36S tests previously proved
# can pile up every frame. Keep all other original sounds untouched.
one("FitdLib/tatou.cpp",
    'void playSound(int num)\n{\n\tif(num == -1)\n\t\treturn;',
    '''void playSound(int num)
{
\tif(num == -1)
\t\treturn;
#ifdef FITD_R36S
    if (num==3 || num==4) {
        static unsigned lastPlayed[2]={0,0};
        static bool initialized[2]={false,false};
        const int which=num-3;
        if(initialized[which] && (unsigned)(timer-lastPlayed[which])<90u)
            return;
        initialized[which]=true;
        lastPlayed[which]=(unsigned)timer;
    }
#endif''',
    "port R36S repeated door sample filter")

# Android TEST11: verify that the old single-slot save implementation actually
# persisted its bytes before reporting success. This retains the proven
# R36S atomic-write and 300-object fixes already installed above.
p=root/"FitdLib/save.cpp"
save=p.read_text()
needle='''    SDL_Log("FITD-ANDROID: save complete slot=%d bytes=%ld",entry,savedBytes);'''
replacement='''    FILE* check=fopen(buffer,"rb");
    if(!check) {
        SDL_Log("FITD-SAVE: ERROR cannot reopen %s after rename",buffer);
        return 0;
    }
    const int seekStatus=fseek(check,0,SEEK_END);
    const long verifiedSize=seekStatus==0?ftell(check):-1;
    fclose(check);
    if(verifiedSize!=savedBytes || verifiedSize<1024) {
        SDL_Log("FITD-SAVE: ERROR mismatched length expected=%ld actual=%ld",
                savedBytes,verifiedSize);
        return 0;
    }
    SDL_Log("FITD-SAVE: VERIFIED slot=%d path=%s bytes=%ld",
            entry,buffer,verifiedSize);'''
assert save.count(needle)==1, "TEST11: save verification hook changed"
save=save.replace(needle,replacement,1)
needle='''int makeSave(int arg0)
{
    return(makeSaveFile(0));
}'''
replacement='''int makeSave(int arg0)
{
    // Original game has one autosave slot; avoid implying a slot selector.
    SDL_Log("FITD-SAVE: request slot=0 current CWD is private app storage");
    const int ok=makeSaveFile(0);
    SDL_Log("FITD-SAVE: completed status=%d",ok);
    return ok;
}'''
assert save.count(needle)==1, "TEST11: makeSave hook changed"
save=save.replace(needle,replacement,1)
needle='''    fHandle = fopen(buffer,"rb");

    if(!fHandle)
    {
        return(0);
    }'''
replacement='''    fHandle = fopen(buffer,"rb");
    if(!fHandle)
    {
        SDL_Log("FITD-SAVE: restore slot=%d not present: %s",saveNumber,buffer);
        return 0;
    }
    SDL_Log("FITD-SAVE: restore opening slot=%d path=%s",saveNumber,buffer);'''
assert save.count(needle)==1, "TEST11: loadSave opening hook changed"
save=save.replace(needle,replacement,1)
p.write_text(save)

p=root/"FitdLib/systemMenu.cpp"
menu=p.read_text()
needle='''                    case 1: // save
                        makeSave(45);
                        break;'''
replacement='''                    case 1: // save
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
                        break;'''
assert menu.count(needle)==1, "TEST11: system menu save item changed"
menu=menu.replace(needle,replacement,1)
p.write_text(menu)
print("FITD-API19: TEST11 save create/read/verify and menu feedback guards")
