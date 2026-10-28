#!/usr/bin/env python3
"""Restore the original HIT_OBJ (action 8) damage/collision path in FITD-R36S.

Logic adapted from spacefarergames/AloneInTheDarkReHaunted
TatouSource/FitdLib/animAction.cpp, case 8 (GPL-2.0).
No particles, blood, HD effects, new rules, forced health changes, or assets.
Requires the prior R36S collision and audio/save protection patches.
"""
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: patch_hit_object_rehaunted.py UPSTREAM_SOURCE_DIR")

path = Path(sys.argv[1]) / "FitdLib/animAction.cpp"
source = path.read_text()
old = """    case 8: // HIT_OBJ
        {
            break;
        }
"""
new = """    case 8: // HIT_OBJ: continuous contact attack (ReHaunted GPL-2.0 adaptation)
        {
            // LM_HIT_OBJECT supplies radius (animActionParam) and damage force.
            // Determine collisions each frame. The life scripts decide whether
            // and how to subtract health; never apply arbitrary damage here.
            const int x = currentProcessedActorPtr->roomX + currentProcessedActorPtr->stepX;
            const int y = currentProcessedActorPtr->roomY + currentProcessedActorPtr->stepY;
            const int z = currentProcessedActorPtr->roomZ + currentProcessedActorPtr->stepZ;
            const int range = currentProcessedActorPtr->animActionParam;

            ZVStruct rangeZv;
            rangeZv.ZVX1 = x - range;
            rangeZv.ZVX2 = x + range;
            rangeZv.ZVY1 = y - range;
            rangeZv.ZVY2 = y + range;
            rangeZv.ZVZ1 = z - range;
            rangeZv.ZVZ2 = z + range;

            const int collision = CheckObjectCol(currentProcessedActorIdx, &rangeZv);
            for (int i = 0; i < collision; ++i)
            {
                const int victimIdx = currentProcessedActorPtr->COL[i];
#ifdef FITD_R36S
                if (victimIdx < 0 || victimIdx >= NUM_MAX_OBJECT)
                {
                    fprintf(stderr,
                        "[FITD-HITOBJ] INVALID_VICTIM attacker=%d world=%d victim=%d slot=%d count=%d time=%u\\n",
                        currentProcessedActorIdx, currentProcessedActorPtr->indexInWorld,
                        victimIdx, i, collision, (unsigned)timer);
                    fflush(stderr);
                    continue;
                }
#endif
                tObject* victim = &ListObjets[victimIdx];
                currentProcessedActorPtr->HIT = victimIdx;
                victim->HIT_BY = currentProcessedActorIdx;
                victim->hitForce = currentProcessedActorPtr->hitForce;
#ifdef FITD_R36S
                // Keep logs bounded; the game can issue HIT_OBJ every frame.
                if (g_gameId == AITD1 && g_currentFloor == 3 &&
                    currentRoom == 3 && (victimIdx == 1 || currentProcessedActorIdx == 1))
                {
                    static unsigned contactHits = 0;
                    ++contactHits;
                    if (contactHits <= 90 || contactHits % 300u == 0)
                    {
                        fprintf(stderr,
                            "[FITD-HITOBJ] CONTACT_DAMAGE event=%u attacker=%d attackerWorld=%d victim=%d victimWorld=%d radius=%d force=%d victimHitBy=%d room=%d floor=%d time=%u\\n",
                            contactHits, currentProcessedActorIdx,
                            currentProcessedActorPtr->indexInWorld,
                            victimIdx, victim->indexInWorld,
                            range, victim->hitForce, victim->HIT_BY,
                            currentRoom, g_currentFloor, (unsigned)timer);
                        fflush(stderr);
                    }
                }
#endif
            }
            break;
        }
"""
if source.count(old) != 1:
    raise RuntimeError("original HIT_OBJ stub not found exactly once; refusing unsafe patch")
if "CONTACT_DAMAGE" in source:
    raise RuntimeError("HIT_OBJ patch already present")
path.write_text(source.replace(old, new, 1))
print("FITD R36S: restored ReHaunted HIT_OBJ contact collision/damage signaling")
