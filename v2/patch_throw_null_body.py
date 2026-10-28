#!/usr/bin/env python3
"""FITD R36S: debug/guard model-body lookup when discarding/throwing an item.

Applied after all established gameplay/audio/save/library patches. Avoids
null sBody dereferences without inventing a 3D mesh or changing saved data.
"""
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: patch_throw_null_body.py PATH_TO_UPSTREAM_SOURCE")
path = Path(sys.argv[1]) / "FitdLib/animAction.cpp"
src = path.read_text()
if "[FITD-THROW] MISSING_BODY" in src:
    raise RuntimeError("throw patch already present")

def once(text, old, new, description):
    if text.count(old) != 1:
        raise RuntimeError(f"{description}: expected one anchor, got {text.count(old)}")
    return text.replace(old, new, 1)

start6 = src.index("case 6: // WAIT_ANIM_THROW")
start7 = src.index("case 7: // THROW", start6)
start8 = src.index("case 8: // HIT_OBJ", start7)
head, block6, block7, tail = src[:start6], src[start6:start7], src[start7:start8], src[start8:]

block6 = once(block6, "                tWorldObject* objPtr = &ListWorldObjets[objIdx];", """#ifdef FITD_R36S
                if (objIdx < 0 || static_cast<size_t>(objIdx) >= ListWorldObjets.size() ||
                    currentProcessedActorPtr->room < 0 ||
                    static_cast<size_t>(currentProcessedActorPtr->room) >= roomDataTable.size()) {
                    fprintf(stderr,
                        "[FITD-THROW] BAD_STATE step=6 actor=%d world=%d obj=%d worldCount=%zu room=%d roomCount=%zu time=%u\\n",
                        currentProcessedActorIdx, currentProcessedActorPtr->indexInWorld,
                        objIdx, ListWorldObjets.size(), currentProcessedActorPtr->room,
                        roomDataTable.size(), static_cast<unsigned>(timer));
                    fflush(stderr);
                    currentProcessedActorPtr->animActionType = 0;
                    return;
                }
#endif
                tWorldObject* objPtr = &ListWorldObjets[objIdx];""", "wait-throw object validation")

block6 = once(block6, "                GiveZVObjet(HQR_Get(HQ_Bodys, objPtr->body),&rangeZv);", """#ifdef FITD_R36S
                sBody* r36sProbeBody = HQR_Get(HQ_Bodys, objPtr->body);
                if (!r36sProbeBody) {
                    fprintf(stderr,
                        "[FITD-THROW] MISSING_BODY step=6 actor=%d world=%d obj=%d body=%d objActor=%d room=%d stage=%d anim=%d frame=%d time=%u; throw cancelled, inventory unchanged\\n",
                        currentProcessedActorIdx, currentProcessedActorPtr->indexInWorld,
                        objIdx, objPtr->body, objPtr->objIndex,
                        currentProcessedActorPtr->room, currentProcessedActorPtr->stage,
                        currentProcessedActorPtr->ANIM, currentProcessedActorPtr->frame,
                        static_cast<unsigned>(timer));
                    fflush(stderr);
                    currentProcessedActorPtr->animActionType = 0;
                    return;
                }
                GiveZVObjet(r36sProbeBody, &rangeZv);
#else
                GiveZVObjet(HQR_Get(HQ_Bodys, objPtr->body),&rangeZv);
#endif""", "wait-throw body lookup")

block7 = once(block7, "            actorIdx = ListWorldObjets[objIdx].objIndex;", """#ifdef FITD_R36S
            if (objIdx < 0 || static_cast<size_t>(objIdx) >= ListWorldObjets.size()) {
                fprintf(stderr, "[FITD-THROW] BAD_STATE step=7 actor=%d obj=%d worldCount=%zu time=%u\\n",
                        currentProcessedActorIdx, objIdx, ListWorldObjets.size(),
                        static_cast<unsigned>(timer));
                fflush(stderr);
                return;
            }
#endif
            actorIdx = ListWorldObjets[objIdx].objIndex;""", "throw object index validation")

block7 = once(block7, "            actorPtr = &ListObjets[actorIdx];", """#ifdef FITD_R36S
            if (actorIdx < 0 || actorIdx >= NUM_MAX_OBJECT) {
                fprintf(stderr, "[FITD-THROW] BAD_ACTOR step=7 obj=%d actor=%d limit=%d time=%u\\n",
                        objIdx, actorIdx, NUM_MAX_OBJECT, static_cast<unsigned>(timer));
                fflush(stderr);
                return;
            }
#endif
            actorPtr = &ListObjets[actorIdx];
#ifdef FITD_R36S
            sBody* r36sFlightBody = HQR_Get(HQ_Bodys, actorPtr->bodyNum);
            if (!r36sFlightBody) {
                fprintf(stderr,
                    "[FITD-THROW] MISSING_BODY step=7 actor=%d world=%d obj=%d thrownActor=%d body=%d room=%d stage=%d time=%u; flight cancelled\\n",
                    currentProcessedActorIdx, currentProcessedActorPtr->indexInWorld,
                    objIdx, actorIdx, actorPtr->bodyNum,
                    currentProcessedActorPtr->room, currentProcessedActorPtr->stage,
                    static_cast<unsigned>(timer));
                fflush(stderr);
                return;
            }
            fprintf(stderr,
                "[FITD-THROW] FLIGHT step=7 actor=%d obj=%d thrownActor=%d body=%d room=%d time=%u\\n",
                currentProcessedActorIdx, objIdx, actorIdx, actorPtr->bodyNum,
                currentProcessedActorPtr->room, static_cast<unsigned>(timer));
            fflush(stderr);
#endif""", "throw flight actor validation")

block7 = once(block7, "            GiveZVObjet(HQR_Get(HQ_Bodys,actorPtr->bodyNum),&actorPtr->zv);", """#ifdef FITD_R36S
            GiveZVObjet(r36sFlightBody, &actorPtr->zv);
#else
            GiveZVObjet(HQR_Get(HQ_Bodys,actorPtr->bodyNum),&actorPtr->zv);
#endif""", "throw flight body lookup")

src = head + block6 + block7 + tail
path.write_text(src)
print("FITD-R36S: safe throw-body lookup and targeted throw telemetry installed")
