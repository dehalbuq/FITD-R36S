#!/usr/bin/env python3
"""R36S: restore orb's requested repeated sample and trace worm-camera / damage path.

Keeps previous stable audio/save/throw protections and screen shake.
"""
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: patch_worm_camera_orb_audio.py UPSTREAM_SOURCE")
root = Path(sys.argv[1])

def replace(rel, old, new, n=1):
    file = root / rel
    text = file.read_text()
    found = text.count(old)
    if found != n:
        raise RuntimeError(f"{rel}: expected {n} anchors, found {found} ({old[:50]!r})")
    file.write_text(text.replace(old, new, n))
    print("patched", rel)

# The orb life-script continuously calls LM_REP_SAMPLE with sample=88.
# Upstream consumes its arguments but never starts the sound. Play at most
# one request per actor per 150 game ticks; never replay every animation frame.
replace("FitdLib/life.cpp",
'''                    const s16 repeatedArg = *(s16*)currentLifePtr;
                    currentLifePtr += 2;
                    if (g_currentFloor == 3 && currentRoom == 3) {''',
'''                    const s16 repeatedArg = *(s16*)currentLifePtr;
                    currentLifePtr += 2;
                    if (g_gameId == AITD1 && g_currentFloor == 3 &&
                        currentRoom == 3 && repeatedSample >= 0 &&
                        currentProcessedActorIdx >= 0 &&
                        currentProcessedActorIdx < NUM_MAX_OBJECT) {
                        static unsigned lastRepeat[NUM_MAX_OBJECT] = {};
                        static unsigned repeatActive[NUM_MAX_OBJECT] = {};
                        const int actorId = currentProcessedActorIdx;
                        if (!repeatActive[actorId] ||
                            (unsigned)(timer - lastRepeat[actorId]) >= 150u) {
                            repeatActive[actorId] = 1;
                            lastRepeat[actorId] = timer;
                            fprintf(stderr,
                                "[FITD-ORB] REP_PLAY actor=%d sample=%d arg=%d room=%d time=%u\\n",
                                actorId, repeatedSample, (int)repeatedArg,
                                currentRoom, (unsigned)timer);
                            fflush(stderr);
                            playSound(repeatedSample);
                        }
                    }
                    if (g_currentFloor == 3 && currentRoom == 3) {''')

# HIT_OBJECT sets action 8, but upstream GereFrappe case 8 is a placeholder.
# Diagnostic only: do not synthesize attacks/damage until the game script
# proves which collision conditions and force should be used.
replace("FitdLib/life.cpp",
'''                currentProcessedActorPtr->hotPointID = -1;
                break;
            }
            case LM_STOP_HIT_OBJECT:''',
'''                currentProcessedActorPtr->hotPointID = -1;
#ifdef FITD_R36S
                if (g_gameId == AITD1 && g_currentFloor == 3 && currentRoom == 3) {
                    static unsigned orbHitObjectCalls = 0;
                    ++orbHitObjectCalls;
                    if (orbHitObjectCalls <= 64 || orbHitObjectCalls % 300 == 0) {
                        fprintf(stderr,
                            "[FITD-ORB] HIT_OBJECT call=%u actor=%d world=%d room=%d flags=%d force=%d colBy=%d hitBy=%d time=%u\\n",
                            orbHitObjectCalls, currentProcessedActorIdx,
                            currentProcessedActorPtr->indexInWorld, currentRoom,
                            lifeTempVar1, lifeTempVar2,
                            currentProcessedActorPtr->COL_BY,
                            currentProcessedActorPtr->HIT_BY,
                            (unsigned)timer);
                        fflush(stderr);
                    }
                }
#endif
                break;
            }
            case LM_STOP_HIT_OBJECT:''')

# Preserve camera decisions as-is while logging the target and each possible
# cover zone. We need to distinguish an abandoned cinematic camera target
# from an out-of-date polygon or invalid geometry.
replace("FitdLib/main.cpp",
'''void GereSwitchCamera(void)
{
\tint localCurrentCam = NumCamera;
\tint newCamera;

\tif(NumCamera!=-1)''',
'''void GereSwitchCamera(void)
{
\tint localCurrentCam = NumCamera;
\tint newCamera;
#ifdef FITD_R36S
    if (g_gameId == AITD1 && g_currentFloor == 5 && currentRoom == 4) {
        static unsigned lastCameraTrace = 0;
        static unsigned cameraTraceCount = 0;
        if (!cameraTraceCount || (unsigned)(timer - lastCameraTrace) >= 100u) {
            lastCameraTrace = timer;
            ++cameraTraceCount;
            const int target = currentCameraTargetActor;
            const bool validTarget = target >= 0 && target < NUM_MAX_OBJECT;
            const tObject* focus = validTarget ? &ListObjets[target] : nullptr;
            const tObject* player = &ListObjets[1];
            int coverCurrent = -1;
            int coverBest = -1;
            if (focus && NumCamera >= 0 && NumCamera < numCameraInRoom &&
                NumCamera < (int)currentCameraZoneList.size() &&
                currentCameraZoneList[NumCamera]) {
                coverCurrent = isInPoly(focus->zv.ZVX1/10, focus->zv.ZVX2/10,
                                        focus->zv.ZVZ1/10, focus->zv.ZVZ2/10,
                                        currentCameraZoneList[NumCamera]);
            }
            if (focus && numCameraInRoom > 0) {
                coverBest = findBestCamera();
            }
            if (cameraTraceCount <= 90 || cameraTraceCount % 10 == 0) {
                fprintf(stderr,
                    "[FITD-WORM-CAM] TRACE n=%u floor=%d room=%d cam=%d newCam=%d cameras=%d target=%d targetWorld=%d targetRoom=%d worldTarget=%d targetXYZ=%d,%d,%d targetZV=%d,%d,%d,%d playerRoom=%d playerXYZ=%d,%d,%d coverCurrent=%d bestCam=%d flagInitView=%d time=%u\\n",
                    cameraTraceCount, g_currentFloor, currentRoom, NumCamera,
                    NewNumCamera, numCameraInRoom, target,
                    focus ? focus->indexInWorld : -1,
                    focus ? focus->room : -1, currentWorldTarget,
                    focus ? focus->roomX : 0, focus ? focus->roomY : 0,
                    focus ? focus->roomZ : 0,
                    focus ? focus->zv.ZVX1 : 0, focus ? focus->zv.ZVX2 : 0,
                    focus ? focus->zv.ZVZ1 : 0, focus ? focus->zv.ZVZ2 : 0,
                    player->room, player->roomX, player->roomY, player->roomZ,
                    coverCurrent, coverBest, FlagInitView, (unsigned)timer);
                fflush(stderr);
            }
        }
    }
#endif

\tif(NumCamera!=-1)''')

replace("FitdLib/life.cpp",
'''                lifeTempVar1 = readNextArgument("Target");

                if (lifeTempVar1 != currentWorldTarget) // same target''',
'''                lifeTempVar1 = readNextArgument("Target");
#ifdef FITD_R36S
                if (g_gameId == AITD1 && g_currentFloor == 5) {
                    fprintf(stderr,
                        "[FITD-WORM-CAM] TARGET_REQUEST floor=%d room=%d requestedWorld=%d oldWorld=%d oldActor=%d callerActor=%d time=%u\\n",
                        g_currentFloor, currentRoom, lifeTempVar1,
                        currentWorldTarget, currentCameraTargetActor,
                        currentProcessedActorIdx, (unsigned)timer);
                    fflush(stderr);
                }
#endif

                if (lifeTempVar1 != currentWorldTarget) // same target''')

print("FITD R36S: orb repeating sample 88 gated; camera and melee impact tracing installed")
