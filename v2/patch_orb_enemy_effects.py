#!/usr/bin/env python3
"""Diagnostic and limited screen-shake restoration for R36S orb enemies.

Applied after established audio/save/library/throw patches; leaves damage and
sound selection unchanged until their script-level causes are confirmed.
"""
from pathlib import Path
import sys
if len(sys.argv) != 2:
    raise SystemExit("usage: patch_orb_enemy_effects.py UPSTREAM_SOURCE_DIR")
root = Path(sys.argv[1])

def edit(rel, changes):
    file = root / rel
    data = file.read_text()
    if '[FITD-ORB]' in data or '[FITD-EFFECT] SHAKE' in data:
        raise RuntimeError(f"already patched: {rel}")
    for title, old, new, expected in changes:
        hits = data.count(old)
        if hits != expected:
            raise RuntimeError(f"{rel}: {title}: expected {expected} anchors, found {hits}")
        data = data.replace(old, new)
    file.write_text(data)
    print(f"patched {rel}")

edit("FitdLib/life.cpp", [
    ("shaking opcode",
     '''                printf("LM_SHAKING\\n");
                //shakingState = shakingAmplitude = *(s16*)(currentLifePtr);
                currentLifePtr += 2;''',
     '''#ifdef FITD_R36S
                const s16 requestedShake = *(s16*)(currentLifePtr);
                if (shakingAmplitude != requestedShake) {
                    static unsigned shakeChanges = 0;
                    ++shakeChanges;
                    if (shakeChanges < 80 || shakeChanges % 256 == 0) {
                        fprintf(stderr,
                            "[FITD-EFFECT] SHAKE old=%d new=%d floor=%d room=%d actor=%d world=%d time=%u\\n",
                            (int)shakingAmplitude, (int)requestedShake, g_currentFloor,
                            currentRoom, currentProcessedActorIdx,
                            currentProcessedActorPtr ? currentProcessedActorPtr->indexInWorld : -1,
                            (unsigned)timer);
                        fflush(stderr);
                    }
                }
                shakingAmplitude = requestedShake;
#else
                printf("LM_SHAKING\\n");
#endif
                currentLifePtr += 2;''', 1),
    ("hit opcode",
     '''                hit(lifeTempVar1, lifeTempVar2, lifeTempVar3, lifeTempVar4, lifeTempVar5, lifeTempVar6);

                break;''',
     '''                hit(lifeTempVar1, lifeTempVar2, lifeTempVar3, lifeTempVar4, lifeTempVar5, lifeTempVar6);
#ifdef FITD_R36S
                if (g_gameId == AITD1 && g_currentFloor == 3 && currentRoom == 3) {
                    static unsigned hitEvents = 0;
                    ++hitEvents;
                    if (hitEvents <= 90 || hitEvents % 240 == 0) {
                        fprintf(stderr,
                            "[FITD-ORB] HIT_SCRIPT call=%u actor=%d world=%d room=%d life=%d anim=%d frame=%d hitRange=%d force=%d resultingAction=%d time=%u\\n",
                            hitEvents, currentProcessedActorIdx,
                            currentProcessedActorPtr->indexInWorld,
                            currentProcessedActorPtr->room,
                            currentProcessedActorPtr->life, lifeTempVar1, lifeTempVar2,
                            lifeTempVar4, lifeTempVar5,
                            currentProcessedActorPtr->animActionType, (unsigned)timer);
                        fflush(stderr);
                    }
                }
#endif
                break;''', 1),
    ("repeat sample unsupported diagnostic",
     '''            case LM_REP_SAMPLE: // sample TODO!
            {
                appendFormated("LM_REP_SAMPLE ");
                if ((g_gameId == AITD1) || (g_gameId == TIMEGATE))
                {
                    evalVar();
                    currentLifePtr += 2;
                }''',
     '''            case LM_REP_SAMPLE: // sample TODO!
            {
                appendFormated("LM_REP_SAMPLE ");
                if ((g_gameId == AITD1) || (g_gameId == TIMEGATE))
                {
#ifdef FITD_R36S
                    const int repeatedSample = evalVar();
                    const s16 repeatedArg = *(s16*)currentLifePtr;
                    currentLifePtr += 2;
                    if (g_currentFloor == 3 && currentRoom == 3) {
                        static unsigned repeatCalls = 0;
                        ++repeatCalls;
                        if (repeatCalls <= 48 || repeatCalls % 240 == 0)
                            fprintf(stderr,
                                "[FITD-ORB] REP_SAMPLE call=%u sample=%d arg=%d actor=%d world=%d room=%d time=%u (upstream TODO)\\n",
                                repeatCalls, repeatedSample, (int)repeatedArg,
                                currentProcessedActorIdx,
                                currentProcessedActorPtr->indexInWorld,
                                currentProcessedActorPtr->room, (unsigned)timer);
                    }
#else
                    evalVar();
                    currentLifePtr += 2;
#endif
                }''', 1)
])

edit("FitdLib/animAction.cpp", [
    ("trace attack collision evaluation",
     '''            collision = CheckObjectCol(currentProcessedActorIdx,&rangeZv);

            for(i=0;i<collision;i++)''',
     '''            collision = CheckObjectCol(currentProcessedActorIdx,&rangeZv);
#ifdef FITD_R36S
            if (g_gameId == AITD1 && g_currentFloor == 3 && currentRoom == 3) {
                static unsigned attackChecks = 0;
                ++attackChecks;
                if (attackChecks <= 100 || attackChecks % 240 == 0) {
                    fprintf(stderr,
                        "[FITD-ORB] HIT_CHECK call=%u attacker=%d world=%d room=%d force=%d range=%d colliders=%d col0=%d col1=%d col2=%d anim=%d frame=%d time=%u\\n",
                        attackChecks, currentProcessedActorIdx,
                        currentProcessedActorPtr->indexInWorld, currentProcessedActorPtr->room,
                        currentProcessedActorPtr->hitForce, range, collision,
                        currentProcessedActorPtr->COL[0], currentProcessedActorPtr->COL[1],
                        currentProcessedActorPtr->COL[2], currentProcessedActorPtr->ANIM,
                        currentProcessedActorPtr->frame, (unsigned)timer);
                    fflush(stderr);
                }
            }
#endif

            for(i=0;i<collision;i++)''', 1)
])

edit("FitdLib/main.cpp", [
    ("player contact tracing (both room paths)",
     '''                currentProcessedActorPtr->COL[currentCollisionSlot++] = i;''',
     '''#ifdef FITD_R36S
                if (g_gameId == AITD1 && g_currentFloor == 3 && currentRoom == 3 &&
                    (i == 1 || actorIdx == 1)) {
                    static unsigned contactCount = 0;
                    ++contactCount;
                    if (contactCount <= 70 || contactCount % 240 == 0) {
                        fprintf(stderr,
                            "[FITD-ORB] CONTACT call=%u actor=%d world=%d fromRoom=%d candidate=%d candidateWorld=%d candidateRoom=%d flags=%d time=%u\\n",
                            contactCount, actorIdx, ListObjets[actorIdx].indexInWorld,
                            actorRoom, i, currentActor->indexInWorld,
                            currentActor->room, currentActor->objectType,
                            (unsigned)timer);
                        fflush(stderr);
                    }
                }
#endif
                currentProcessedActorPtr->COL[currentCollisionSlot++] = i;''', 2)
])

edit("FitdLib/bgfxGlue.cpp", [
    ("expose shaking state",
     '''int outputResolution[2] = { -1, -1 };

void StartFrame()''',
     '''int outputResolution[2] = { -1, -1 };
#ifdef FITD_R36S
extern short shakingAmplitude; // s16 stored in the original save-game state
#endif

void StartFrame()''', 1),
    ("apply small bounded viewport shake",
     '''    bgfx::setViewRect(0, 0, 0, outputResolution[0], outputResolution[1]);
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH);''',
     '''#ifdef FITD_R36S
    // Original game LM_SHAKING was a no-op in the upstream renderer.
    // Shake the whole game image (background and actors together); keep
    // the amplitude small to avoid exposing large borders on the R36S.
    // This is a first compatibility implementation, not pixel-perfect DOS.
    static unsigned shakeFrame = 0;
    int magnitude = (int)shakingAmplitude;
    if (magnitude < 0) magnitude = -magnitude;
    int shift = (magnitude + 63) / 64;
    if (shift > 5) shift = 5;
    const unsigned phase = shakeFrame++;
    const int dx = shift && (phase & 1u) ? shift : 0;
    const int dy = shift && (phase & 2u) ? shift : 0;
    const int width = outputResolution[0] > dx ? outputResolution[0] - dx : 1;
    const int height = outputResolution[1] > dy ? outputResolution[1] - dy : 1;
    bgfx::setViewRect(0, (uint16_t)dx, (uint16_t)dy,
                      (uint16_t)width, (uint16_t)height);
#else
    bgfx::setViewRect(0, 0, 0, outputResolution[0], outputResolution[1]);
#endif
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH);''', 1)
])
print("FITD R36S: orb hit/contact/repeat-sample diagnostic + limited screen shake installed")
