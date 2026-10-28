#!/usr/bin/env python3
"""R36S / FITD: library secret-door invalid-room collision diagnostic.

Applied after the existing ARM64, save, and audio patches.
Never modifies room indices or moves actors. Bad collision candidates are
skipped and precisely logged; unrelated room transforms fail closed.
"""
import sys
from pathlib import Path

if len(sys.argv) != 2:
    raise SystemExit("usage: patch_library_room_crash.py PATH_TO_UPSTREAM_SOURCE")
main = Path(sys.argv[1]) / "FitdLib/main.cpp"
src = main.read_text()
if "[FITD-LIBRARY] INVALID_ROOM" in src:
    raise SystemExit("library room diagnostic already installed")

adjust = "void AdjustZV(ZVStruct* zvPtr, int startRoom, int destRoom)\n{"
collision = "int CheckObjectCol(int actorIdx, ZVStruct* zvPtr)\n{"
if src.count(adjust) != 1 or src.count(collision) != 1:
    raise RuntimeError("expected room conversion/collision functions missing")

reporter = r'''#ifdef FITD_R36S
// This log is deliberately limited so a corrupt actor does not fill the SD card.
static bool r36sValidRoom(int id)
{
    return id >= 0 && static_cast<size_t>(id) < roomDataTable.size();
}

static void r36sInvalidRoom(const char* site, int actorId, int candidateId,
                            int fromRoom, int toRoom)
{
    static unsigned faultCount = 0;
    const unsigned event = ++faultCount;
    if (event > 64u && event % 512u)
        return;
    const bool actorOk = actorId >= 0 && actorId < NUM_MAX_OBJECT;
    const bool candidateOk = candidateId >= 0 && candidateId < NUM_MAX_OBJECT;
    const tObject* actor = actorOk ? &ListObjets[actorId] : nullptr;
    const tObject* candidate = candidateOk ? &ListObjets[candidateId] : nullptr;
    fprintf(stderr,
        "[FITD-LIBRARY] INVALID_ROOM event=%u site=%s floor=%d activeRoom=%d count=%zu "
        "from=%d to=%d actor=%d world=%d actorRoom=%d life=%d anim=%d frame=%d "
        "mode=%d posXZ=%d,%d candidate=%d candidateWorld=%d candidateRoom=%d "
        "candidateFlags=%d currentProcessed=%d time=%u\\n",
        event, site, g_currentFloor, currentRoom, roomDataTable.size(),
        fromRoom, toRoom, actorId,
        actor ? actor->indexInWorld : -999,
        actor ? actor->room : -999, actor ? actor->life : -999,
        actor ? actor->ANIM : -999, actor ? actor->frame : -999,
        actor ? actor->trackMode : -999,
        actor ? actor->roomX : -999, actor ? actor->roomZ : -999,
        candidateId,
        candidate ? candidate->indexInWorld : -999,
        candidate ? candidate->room : -999,
        candidate ? candidate->objectType : -999,
        currentProcessedActorIdx, static_cast<unsigned>(timer));
    fflush(stderr);
}
#endif

'''
src = src.replace(adjust, reporter + adjust + '''
#ifdef FITD_R36S
    if (!r36sValidRoom(startRoom) || !r36sValidRoom(destRoom)) {
        r36sInvalidRoom("AdjustZV", currentProcessedActorIdx, -1,
                        startRoom, destRoom);
        // No legal coordinate transform exists. Avoid out-of-range access;
        // callers needing collision answers are guarded in CheckObjectCol.
        return;
    }
#endif
''', 1)

src = src.replace(collision, collision + '''
#ifdef FITD_R36S
    if (actorIdx < 0 || actorIdx >= NUM_MAX_OBJECT) {
        r36sInvalidRoom("CheckObjectCol-invalid-actor", actorIdx, -1,
                        -1, -1);
        return 0;
    }
#endif
''', 1)

start = src.index(collision)
end = src.index("void take(int objIdx)", start)
body = src[start:end]
anchor = "int actorRoom = ListObjets[actorIdx].room;"
if body.count(anchor) != 1:
    raise RuntimeError("actor room assignment changed")
body = body.replace(anchor, anchor + '''
#ifdef FITD_R36S
    if (!r36sValidRoom(actorRoom)) {
        r36sInvalidRoom("CheckObjectCol-source", actorIdx, -1,
                        actorRoom, currentRoom);
        return 0;
    }
#endif
''', 1)
anchor = "if (currentActor->room != actorRoom)\n        {"
if body.count(anchor) != 1:
    raise RuntimeError("cross-room candidate branch changed")
body = body.replace(anchor, anchor + '''
#ifdef FITD_R36S
            if (!r36sValidRoom(currentActor->room)) {
                r36sInvalidRoom("CheckObjectCol-candidate", actorIdx, i,
                                actorRoom, currentActor->room);
                // This candidate has no valid room coordinate system:
                // do not invent a collision with the player or another actor.
                continue;
            }
#endif
''', 1)
src = src[:start] + body + src[end:]
main.write_text(src)
print("FITD R36S: library collision diagnostic and invalid-room guards installed")
