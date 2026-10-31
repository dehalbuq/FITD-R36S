#!/usr/bin/env python3
"""R36S regression telemetry for the scripted 2-barrel cellar exit.

Source baseline: yaz0r/FITD 543e22bc.
Compare: spacefarergames/AloneInTheDarkReHaunted GereDec in main.cpp
is nearly identical (apart from bounds protection); no safe player relocation
fix can be inferred without collision and position traces.

This patch ONLY logs. It does not change physics, object flags, life scripts,
player coordinates, save state, or animation timing.
"""
from pathlib import Path
import sys
if len(sys.argv) != 2:
    raise SystemExit("usage: patch_cellar_barrel_stuck.py UPSTREAM_SOURCE_DIR")
p = Path(sys.argv[1]) / "FitdLib/main.cpp"
s = p.read_text()
old = '''void GereDec()
{
\tbool onceMore = false;'''
new = '''void GereDec()
{
#ifdef FITD_R36S
    // Entered from the lower secret passage: stage 4 / room 0. The 2 scripted
    // barrels in this room are world objects 201 and 202.
    if (g_gameId == AITD1 && g_currentFloor == 4 &&
        currentRoom == 0 && currentProcessedActorPtr &&
        currentProcessedActorPtr->indexInWorld == 1) {
        static unsigned cellarTraceCalls = 0;
        static unsigned lastCellarTraceTimer = 0;
        static int oldX = 0, oldY = 0, oldZ = 0;
        static bool haveOldPos = false;
        const unsigned tick = (unsigned)timer;
        ++cellarTraceCalls;
        tObject* player = currentProcessedActorPtr;
        const int effectiveX = player->roomX + player->stepX;
        const int effectiveY = player->roomY + player->stepY;
        const int effectiveZ = player->roomZ + player->stepZ;
        const bool moved = !haveOldPos || effectiveX != oldX ||
                           effectiveY != oldY || effectiveZ != oldZ;
        const bool sample = cellarTraceCalls <= 40 ||
            tick < lastCellarTraceTimer || tick - lastCellarTraceTimer >= 20;
        if (sample) {
            lastCellarTraceTimer = tick;
            fprintf(stderr,
                "[FITD-BARRELS] PLAYER actor=%d room=%d floor=%d cam=%d xyz=%d,%d,%d step=%d,%d,%d effective=%d,%d,%d moved=%d hardCol=%d hardDec=%d colBy=%d col0=%d col1=%d col2=%d type=0x%x dyn=0x%x life=%d mode=%d anim=%d frame=%d track=%d pos=%d time=%u\\n",
                currentProcessedActorIdx, player->room, player->stage,
                NumCamera, player->roomX, player->roomY, player->roomZ,
                player->stepX, player->stepY, player->stepZ,
                effectiveX, effectiveY, effectiveZ, moved ? 1 : 0,
                player->HARD_COL, player->HARD_DEC, player->COL_BY,
                player->COL[0], player->COL[1], player->COL[2],
                (unsigned)player->objectType, (unsigned)player->dynFlags,
                player->life, player->lifeMode, player->ANIM,
                player->frame, player->trackMode, player->positionInTrack, tick);

            // Capture whether the two barrels are actors and if the player
            // overlaps their physical bounds after the scripted animation.
            for (int barrelWorld = 201; barrelWorld <= 202; ++barrelWorld) {
                if ((size_t)barrelWorld >= ListWorldObjets.size())
                    continue;
                const tWorldObject& world = ListWorldObjets[barrelWorld];
                const int actor = world.objIndex;
                fprintf(stderr,
                    "[FITD-BARRELS] WORLD id=%d actor=%d stage=%d room=%d life=%d mode=%d body=%d flags=0x%x pos=%d,%d,%d time=%u\\n",
                    barrelWorld, actor, world.stage, world.room,
                    world.life, world.lifeMode, world.body,
                    (unsigned)world.flags, world.x, world.y, world.z, tick);
                if (actor >= 0 && actor < NUM_MAX_OBJECT) {
                    tObject* barrel = &ListObjets[actor];
                    const bool sameRoom = barrel->room == player->room;
                    const int overlap = sameRoom ?
                        CubeIntersect(&player->zv, &barrel->zv) : 0;
                    fprintf(stderr,
                        "[FITD-BARRELS] ACTOR id=%d world=%d sameRoom=%d overlap=%d xyz=%d,%d,%d step=%d,%d,%d zvXZ=%d,%d,%d,%d type=0x%x dyn=0x%x hard=%d life=%d anim=%d time=%u\\n",
                        actor, barrel->indexInWorld, sameRoom ? 1 : 0, overlap,
                        barrel->roomX, barrel->roomY, barrel->roomZ,
                        barrel->stepX, barrel->stepY, barrel->stepZ,
                        barrel->zv.ZVX1, barrel->zv.ZVX2,
                        barrel->zv.ZVZ1, barrel->zv.ZVZ2,
                        (unsigned)barrel->objectType,
                        (unsigned)barrel->dynFlags, barrel->HARD_COL,
                        barrel->life, barrel->ANIM, tick);
                }
            }

            // Detect direct overlap with hard collisions in the cellar; the
            // underlying engine's collision resolver remains unchanged.
            const int roomIndex = player->room;
            if (roomIndex >= 0 && (size_t)roomIndex < roomDataTable.size()) {
                roomDataStruct& roomData = roomDataTable[roomIndex];
                int hitCount = 0;
                for (size_t i = 0; i < roomData.hardColTable.size(); ++i) {
                    hardColStruct& hc = roomData.hardColTable[i];
                    if (CubeIntersect(&player->zv, &hc.zv)) {
                        if (hitCount < 6)
                            fprintf(stderr,
                                "[FITD-BARRELS] HARD_OVERLAP hardIndex=%zu type=%u param=%u hardXZ=%d,%d,%d,%d playerXZ=%d,%d,%d,%d time=%u\\n",
                                i, hc.type, hc.parameter,
                                hc.zv.ZVX1, hc.zv.ZVX2,
                                hc.zv.ZVZ1, hc.zv.ZVZ2,
                                player->zv.ZVX1, player->zv.ZVX2,
                                player->zv.ZVZ1, player->zv.ZVZ2, tick);
                        ++hitCount;
                    }
                }
                fprintf(stderr,
                    "[FITD-BARRELS] HARD_SUMMARY count=%d total=%zu time=%u\\n",
                    hitCount, roomData.hardColTable.size(), tick);
            }
            fflush(stderr);
        }
        oldX = effectiveX; oldY = effectiveY; oldZ = effectiveZ;
        haveOldPos = true;
    }
#endif
\tbool onceMore = false;'''
if s.count(old) != 1:
    raise RuntimeError(f"GereDec unique anchor expected, found {s.count(old)}")
s = s.replace(old, new, 1)
p.write_text(s)
print("FITD-R36S: cellars barrel/player collision traces installed; physics unchanged")
