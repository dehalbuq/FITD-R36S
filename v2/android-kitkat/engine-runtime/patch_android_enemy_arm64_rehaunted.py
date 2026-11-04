#!/usr/bin/env python3
"""Android API19: source-pinned R36S ARM64 enemy fixes + Re-Haunted GPLv2 AI fixes.

Designed for FITD yaz0r 543e22bc after the existing gameplay, cellar and
Re-Haunted HIT_OBJ patches. No SDL3, bgfx, modern HD assets or game files.
Upstream code: yaz0r/FITD (GPLv2).
Compared with spacefarergames/AloneInTheDarkReHaunted
TatouSource/FitdLib/{main,anim,life}.cpp (GPLv2).
Only AITD1 scripted window followers (world IDs 21 and 57) are eligible for
the R36S window-collision recovery, which ARM64 enables via
FITD_WINDOW_RECOVERY=1. General NPCs get Re-Haunted's mode/speed corrections.
"""
import sys
from pathlib import Path

if len(sys.argv)!=2:
    raise SystemExit("usage: patch_android_enemy_arm64_rehaunted.py SOURCE_ROOT")
root=Path(sys.argv[1])
def once(path, old, new, label):
    f=root / path
    value=f.read_text()
    n=value.count(old)
    if n!=1:
        raise RuntimeError(f"TEST19 {label}: {path}, anchor count={n}")
    f.write_text(value.replace(old,new,1))
    print("FITD-TEST19:",label)

# Re-Haunted: reset stale speed transition when AI changes between follow and
# patrol track modes. Without this the actor may remain in a low/zero speed
# interpolation from the previous life script state.
once("FitdLib/main.cpp",
'''\t\t\tcurrentProcessedActorPtr->MARK = -1;
\t\t\tbreak;
\t\t}
\tcase 3:''',
'''\t\t\tcurrentProcessedActorPtr->MARK = -1;
#ifdef FITD_R36S
            currentProcessedActorPtr->speedChange.numSteps=0;
            currentProcessedActorPtr->speedChange.endValue=0;
#endif
\t\t\tbreak;
\t\t}
\tcase 3:''',
"Re-Haunted follow mode speed interpolator reset")
once("FitdLib/main.cpp",
'''\t\t\tcurrentProcessedActorPtr->positionInTrack = 0;
\t\t\tcurrentProcessedActorPtr->MARK = -1;
\t\t\tbreak;''',
'''\t\t\tcurrentProcessedActorPtr->positionInTrack = 0;
\t\t\tcurrentProcessedActorPtr->MARK = -1;
#ifdef FITD_R36S
            currentProcessedActorPtr->speedChange.numSteps=0;
            currentProcessedActorPtr->speedChange.endValue=0;
#endif
\t\t\tbreak;''',
"Re-Haunted patrol mode speed interpolator reset")

# ARM64: transient AF_SPECIAL actors are effects, not physical obstacles.
# Ghost particles used to occupy one of three available collision slots and
# suppress physical movement/pathfinding despite being invisible.
p=root/"FitdLib/main.cpp"
source=p.read_text()
idx=source.index("int CheckObjectCol(int actorIdx, ZVStruct* zvPtr)")
end=source.index("void take(int objIdx)",idx)
block=source[idx:end]
needle='''        if (currentActor->indexInWorld == -1)
            continue;'''
repl='''        if (currentActor->indexInWorld == -1 ||
            (currentActor->objectType & AF_SPECIAL))
            continue;
#ifdef FITD_R36S
        // ARM64 window recovery: ignore the scripted broken-window object
        // only for its OWN nearby active pursuer, not the player or bullets.
        const tObject& walker=ListObjets[actorIdx];
        const bool windowRemnant =
            (walker.indexInWorld==21 && walker.room==0 &&
             currentActor->indexInWorld==7 && currentActor->room==0) ||
            (walker.indexInWorld==57 && walker.room==4 &&
             currentActor->indexInWorld==51 && currentActor->room==4);
        if(g_gameId==AITD1 && windowRemnant && walker.trackMode==2 &&
           walker.speed>=4 &&
           abs(walker.roomX-currentActor->roomX)<900 &&
           abs(walker.roomZ-currentActor->roomZ)<900) {
            static unsigned suppressedWindow[2]={};
            const int slot=walker.indexInWorld==21?0:1;
            if(++suppressedWindow[slot]<=3)
                SDL_Log("FITD-NPC: WINDOW_PASS followerWorld=%d windowWorld=%d room=%d",
                        walker.indexInWorld,currentActor->indexInWorld,walker.room);
            continue;
        }
#endif'''
if block.count(needle)!=1:raise RuntimeError("TEST19 CheckObjectCol actor collision guard changed")
p.write_text(source[:idx]+block.replace(needle,repl,1)+source[end:])
print("FITD-TEST19: ARM64 special-effects actor skip + targeted window pursuers")

# ARM64: unsupported particle modes must not reserve dead AF_SPECIAL actors.
once("FitdLib/life.cpp",
'''int InitSpecialObjet(int mode, int X, int Y, int Z, int stage, int room, int alpha, int beta, int gamma, ZVStruct* zvPtr)
{
    s16 localSpecialTable[4];''',
'''int InitSpecialObjet(int mode, int X, int Y, int Z, int stage, int room, int alpha, int beta, int gamma, ZVStruct* zvPtr)
{
#ifdef FITD_R36S
    if(mode!=0)return -1; // unsupported particles must not block real NPCs
#endif
    s16 localSpecialTable[4];''',
"ARM64: unimplemented particle ghost actors are never spawned")
once("FitdLib/life.cpp",
'''                case 1: // flow
                {
                    currentProcessedActorPtr = &ListObjets[currentProcessedActorPtr->HIT_BY];''',
'''                case 1: // flow
                {
#ifdef FITD_R36S
                    break; // no unsupported HIT_BY dereference
#endif
                    currentProcessedActorPtr = &ListObjets[currentProcessedActorPtr->HIT_BY];''',
"ARM64: prevent invalid particle HIT_BY dereference")

# Re-Haunted: if an animated FOLLOW NPC's animation has near-zero root
# translation, inject the script's speed interpolator into walking movement.
# AITD1's player uses trackMode=1 and is deliberately unaffected.
# Time ramp 72 ticks ~= original 30 ticks at 25Hz on this 60Hz engine.
once("FitdLib/anim.cpp",
'''\t\tcurrentProcessedActorPtr->END_FRAME = SetInterAnimObjet(currentProcessedActorPtr->frame, HQR_Get(HQ_Anims, currentProcessedActorPtr->ANIM), HQR_Get(HQ_Bodys, currentProcessedActorPtr->bodyNum));

\t\twalkStep(animStepX,animStepZ,currentProcessedActorPtr->beta);''',
'''\t\tcurrentProcessedActorPtr->END_FRAME = SetInterAnimObjet(currentProcessedActorPtr->frame, HQR_Get(HQ_Anims, currentProcessedActorPtr->ANIM), HQR_Get(HQ_Bodys, currentProcessedActorPtr->bodyNum));
#ifdef FITD_R36S
        if(currentProcessedActorPtr->trackMode==2 &&
           currentProcessedActorPtr->indexInWorld>=0 &&
           currentProcessedActorPtr->speed>0) {
            RealValue* ramp=&currentProcessedActorPtr->speedChange;
            if(ramp->endValue!=currentProcessedActorPtr->speed) {
                InitRealValue(0,currentProcessedActorPtr->speed,72,ramp);
            }
            // Unlike unconditional reinit every time numSteps reaches 0,
            // evaluateReal() returns endValue continuously after the ramp.
            animStepZ+=evaluateReal(ramp);
        }
#endif
\t\twalkStep(animStepX,animStepZ,currentProcessedActorPtr->beta);''',
"Re-Haunted animated NPC follow-speed movement, fixed 60Hz ramp")

# Actor motion diagnostics and bounded R36S hard-wall recovery, ONLY for the
# two exact AITD1 window monsters where ARM64 previously observed collision
# overlap. Nothing changes in general physics, other enemies or camera.
once("FitdLib/anim.cpp",
'''\ts16 localTable[3];
\tZVStruct zvLocal;''',
'''\ts16 localTable[3];
#ifdef FITD_R36S
    int r36sNpcWallHits=0, r36sNpcRequestedX=0, r36sNpcRequestedZ=0;
#endif
\tZVStruct zvLocal;''',
"NPC collision/step tracking variables")
once("FitdLib/anim.cpp",
'''\tmemcpy(localTable,currentProcessedActorPtr->COL,6);
\t
\tif(stepX || stepY || stepZ) // start of movement management''',
'''\tmemcpy(localTable,currentProcessedActorPtr->COL,6);
#ifdef FITD_R36S
    r36sNpcRequestedX=stepX;
    r36sNpcRequestedZ=stepZ;
#endif
\t
\tif(stepX || stepY || stepZ) // start of movement management''',
"NPC requested motion captured before collisions")
once("FitdLib/anim.cpp",
'''\t\t\tint numCol = AsmCheckListCol(&zvLocal, &roomDataTable[currentProcessedActorPtr->room]);

\t\t\tfor(int i=0;i<numCol;i++)''',
'''\t\t\tint numCol = AsmCheckListCol(&zvLocal, &roomDataTable[currentProcessedActorPtr->room]);
#ifdef FITD_R36S
            r36sNpcWallHits=numCol;
#endif

\t\t\tfor(int i=0;i<numCol;i++)''',
"NPC physical wall collisions captured")

# Separate file augmentation after existing R36S cellar wall recovery.
p=root/"FitdLib/anim.cpp"
s=p.read_text()
needle='\t\tcurrentProcessedActorPtr->stepX = stepX + oldStepX;'
if s.count(needle)!=1:raise RuntimeError("TEST19 GereAnim final step anchor missing")
recovery=r'''#ifdef FITD_R36S
        // Limited ARM64-style window-monster escape. A 60Hz AI running
        // in place for at least 60 ticks (>1s) against a hard wall is a
        // potential softlock. Search nearby *collision-free* cells ahead,
        // never move the player and never weaken ordinary walls.
        {
            static unsigned blockedTicks[NUM_MAX_OBJECT]={};
            static short trackedWorld[NUM_MAX_OBJECT]={};
            static unsigned recoveryEvents[NUM_MAX_OBJECT]={};
            const int actorId=currentProcessedActorIdx;
            if(actorId>=0 && actorId<NUM_MAX_OBJECT){
                const int world=currentProcessedActorPtr->indexInWorld;
                const int room=currentProcessedActorPtr->room;
                const bool windowFollower=g_gameId==AITD1 &&
                    ((world==21 && room==0) || (world==57 && room==4)) &&
                    currentProcessedActorPtr->trackMode==2 &&
                    currentProcessedActorPtr->speed>=4 &&
                    room>=0 && (size_t)room<roomDataTable.size();
                if(trackedWorld[actorId]!=world){
                    blockedTicks[actorId]=0;
                    recoveryEvents[actorId]=0;
                    trackedWorld[actorId]=(short)world;
                }
                if(windowFollower && (r36sNpcRequestedX||r36sNpcRequestedZ) &&
                   stepX==0 && stepZ==0 && r36sNpcWallHits>0 &&
                   recoveryEvents[actorId]<2){
                    if(++blockedTicks[actorId]>=60){
                        blockedTicks[actorId]=0;
                        ++recoveryEvents[actorId];
                        const int oldX=currentProcessedActorPtr->roomX;
                        const int oldZ=currentProcessedActorPtr->roomZ;
                        const ZVStruct original=currentProcessedActorPtr->zv;
                        const tObject* player=(currentCameraTargetActor>=0 &&
                            currentCameraTargetActor<NUM_MAX_OBJECT)
                            ? &ListObjets[currentCameraTargetActor]:nullptr;
                        bool found=false;
                        int bestDX=0,bestDZ=0;
                        // R36S recovery search is bounded to 1200 world units.
                        for(int radius=120;radius<=1200 && !found;radius+=120){
                            for(int dx=-radius;dx<=radius && !found;dx+=120){
                                for(int dz=-radius;dz<=radius;dz+=120){
                                    if(abs(dx)!=radius && abs(dz)!=radius)continue;
                                    if(player && player->room==room &&
                                       ((long long)dx*(player->roomX-oldX)+
                                        (long long)dz*(player->roomZ-oldZ))<=0)
                                        continue;
                                    ZVStruct box=original;
                                    box.ZVX1+=dx;box.ZVX2+=dx;
                                    box.ZVZ1+=dz;box.ZVZ2+=dz;
                                    if(AsmCheckListCol(&box,&roomDataTable[room])>0)
                                        continue;
                                    bool occupied=false;
                                    for(int id=0;id<NUM_MAX_OBJECT;id++){
                                        if(id==actorId)continue;
                                        const tObject& other=ListObjets[id];
                                        if(other.indexInWorld<0 ||
                                           (other.objectType & AF_SPECIAL) ||
                                           other.room<0 ||
                                           (size_t)other.room>=roomDataTable.size() ||
                                           (world==21 && other.indexInWorld==7) ||
                                           (world==57 && other.indexInWorld==51))
                                            continue;
                                        ZVStruct otherBox=other.zv;
                                        if(other.room!=room)
                                            AdjustZV(&otherBox,other.room,room);
                                        if(CubeIntersect(&box,&otherBox)){
                                            occupied=true;break;
                                        }
                                    }
                                    if(occupied)continue;
                                    bestDX=dx;bestDZ=dz;found=true;break;
                                }
                            }
                        }
                        if(found){
                            currentProcessedActorPtr->roomX+=bestDX;
                            currentProcessedActorPtr->worldX+=bestDX;
                            currentProcessedActorPtr->roomZ+=bestDZ;
                            currentProcessedActorPtr->worldZ+=bestDZ;
                            currentProcessedActorPtr->zv.ZVX1+=bestDX;
                            currentProcessedActorPtr->zv.ZVX2+=bestDX;
                            currentProcessedActorPtr->zv.ZVZ1+=bestDZ;
                            currentProcessedActorPtr->zv.ZVZ2+=bestDZ;
                            currentProcessedActorPtr->animNegX=0;
                            currentProcessedActorPtr->animNegZ=0;
                            SDL_Log("FITD-NPC: UNSTICK world=%d actor=%d room=%d "
                                    "from=%d,%d offset=%d,%d wallHits=%d",
                                    world,actorId,room,oldX,oldZ,bestDX,bestDZ,
                                    r36sNpcWallHits);
                        }else{
                            SDL_Log("FITD-NPC: UNSTICK_NO_FREE_POSITION world=%d actor=%d room=%d",
                                    world,actorId,room);
                        }
                    }
                }else blockedTicks[actorId]=0;
                // Trace nearby active NPCs without requiring a launcher flag.
                // Limit to one report per 3 s per slot.
                if(windowFollower){
                    static Uint32 lastTraceMs[NUM_MAX_OBJECT]={};
                    const Uint32 now=SDL_GetTicks();
                    if(now-lastTraceMs[actorId]>=3000){
                        lastTraceMs[actorId]=now;
                        SDL_Log("FITD-NPC: TRACK world=%d actor=%d room=%d "
                                "track=%d speed=%d anim=%d frame=%d xyz=%d,%d "
                                "requested=%d,%d resolved=%d,%d walls=%d",
                                world,actorId,room,currentProcessedActorPtr->trackMode,
                                currentProcessedActorPtr->speed,currentProcessedActorPtr->ANIM,
                                currentProcessedActorPtr->frame,
                                currentProcessedActorPtr->roomX,currentProcessedActorPtr->roomZ,
                                r36sNpcRequestedX,r36sNpcRequestedZ,stepX,stepZ,
                                r36sNpcWallHits);
                    }
                }
            }
        }
#endif
'''
s=s.replace(needle,recovery+needle,1)
p.write_text(s)
print("FITD-TEST19: R36S window monsters 21/57 bounded wall recovery and AI trace")

# Projectile ray correctness + bounded no-progress loop from ARM64.
once("FitdLib/main.cpp",
'if(AsmCheckListCol(&localZv, &roomDataTable[room]) <= 0)',
'if(AsmCheckListCol(&localZv, &roomDataTable[room]) > 0)',
"R36S projectile stops on actual walls (ray boolean correction)")
once("FitdLib/main.cpp",
'\twalkStep(param * 2, 0, beta);',
'''\twalkStep(param * 2, 0, beta);
#ifdef FITD_R36S
    unsigned r36sBulletSteps=0;
#endif''',
"ARM64 raycast iteration cap: initialize")
once("FitdLib/main.cpp",
'\twhile(foundFlag == -2)\n\t{\n\t\tlocalZv.ZVX1 += animMoveX;',
'''\twhile(foundFlag == -2)
\t{
#ifdef FITD_R36S
        if(++r36sBulletSteps>512u){foundFlag=-1;break;}
#endif
\t\tlocalZv.ZVX1 += animMoveX;''',
"ARM64 raycast iteration cap: stop after 512 steps")

# Re-Haunted: LM_CONTINUE_TRACK must reject missing track state/resources
# instead of dereferencing null HQR archives. Preserve valid behavior.
once("FitdLib/life.cpp",
'''                ptr = HQR_Get(listTrack, currentProcessedActorPtr->trackNumber);

                ptr += currentProcessedActorPtr->positionInTrack * 2;''',
'''#ifdef FITD_R36S
                if(currentProcessedActorPtr->trackNumber<0 ||
                   currentProcessedActorPtr->positionInTrack<0) {
                    SDL_Log("FITD-AI: CONTINUE_TRACK invalid track=%d pos=%d actor=%d",
                            currentProcessedActorPtr->trackNumber,
                            currentProcessedActorPtr->positionInTrack,
                            currentProcessedActorIdx);
                    break;
                }
#endif
                ptr = HQR_Get(listTrack, currentProcessedActorPtr->trackNumber);
#ifdef FITD_R36S
                if(!ptr) {
                    SDL_Log("FITD-AI: CONTINUE_TRACK missing track resource=%d",
                            currentProcessedActorPtr->trackNumber);
                    break;
                }
#endif
                ptr += currentProcessedActorPtr->positionInTrack * 2;''',
"Re-Haunted missing-track and invalid-index guards")

# Idle object-list invalidation is localized, no new rendering pipeline:
# a vanishing draw list triggers one rebuild through the existing engine path.
once("FitdLib/mainLoop.cpp",
'''        sortActorList();

        handleAnim2d();''',
'''#ifdef FITD_R36S
        if(NbAffObjets==0 && currentRoom>=0 && !FlagChangeSalle) {
            static Uint32 lastEmptyReport=0;
            const Uint32 now=SDL_GetTicks();
            if(now-lastEmptyReport>=3000) {
                lastEmptyReport=now;
                SDL_Log("FITD-AI: empty draw list; regenerating visible objects room=%d",
                        currentRoom);
            }
            GenereActiveList();
            GenereAffList();
        }
#endif
        sortActorList();

        handleAnim2d();''',
"Re-Haunted empty actor draw-list fallback")

print("FITD-TEST19: ARM64 actor/shot safety + Re-Haunted AI/root motion applied")
