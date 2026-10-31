#!/usr/bin/env python3
"""Narrow AITD1 cellar wall-overlap recovery after secret passage.

The 2026-10-31 R36S diagnostic log proves that the player saved at
(6759, 0, 4794) intersects hard wall #2 (Z=5000..5300) by 60 world units.
Barrels world 201 and 202 are NOT intersecting (both overlap=0).
Only resolve that verified, idle, room-0 spawn state by the smallest Z
translation needed to clear the wall + a small 8-unit margin.
No actor collision flags, script opcodes, animations, room data or
other locations are modified. Works also on SAVE0.ITD already stuck.
"""
import sys
from pathlib import Path
if len(sys.argv) != 2:
    raise SystemExit("usage: patch_cellar_wall_recovery.py UPSTREAM_SOURCE_DIR")
p = Path(sys.argv[1]) / "FitdLib/main.cpp"
s = p.read_text()
old = '''void GereDec()
{
#ifdef FITD_R36S
    // Entered from the lower secret passage: stage 4 / room 0.'''
new = '''void GereDec()
{
#ifdef FITD_R36S
    // Original secret-passage endpoint / saved softlock only. The wall is
    // solid and must remain solid; move the actor just outside its ZV.
    // This block runs before the room-zone test and never touches other rooms.
    if (g_gameId == AITD1 && g_currentFloor == 4 && currentRoom == 0 &&
        currentProcessedActorPtr &&
        currentProcessedActorPtr->indexInWorld == 1 &&
        currentProcessedActorPtr->life == 549 &&
        currentProcessedActorPtr->roomX >= 6720 &&
        currentProcessedActorPtr->roomX <= 6800 &&
        currentProcessedActorPtr->roomZ >= 4750 &&
        currentProcessedActorPtr->roomZ <= 4850 &&
        currentProcessedActorPtr->stepX == 0 &&
        currentProcessedActorPtr->stepZ == 0 &&
        (currentProcessedActorPtr->ANIM == 4 ||
         currentProcessedActorPtr->ANIM == 5) &&
        roomDataTable.size() > 0 &&
        roomDataTable[0].hardColTable.size() > 2) {
        tObject* player = currentProcessedActorPtr;
        hardColStruct* wall = &roomDataTable[0].hardColTable[2];
        // Guard geometry to avoid coupling to another edition/data set.
        if (wall->type == 1 &&
            wall->zv.ZVZ1 == 5000 && wall->zv.ZVZ2 == 5300 &&
            CubeIntersect(&player->zv, &wall->zv)) {
            const int dz = wall->zv.ZVZ1 - player->zv.ZVZ2 - 8;
            if (dz < 0 && dz >= -100) {
                fprintf(stderr,
                    "[FITD-BARRELS] RECOVER_WALL actor=%d before=%d,%d,%d dz=%d wallZ=%d,%d zvZ=%d,%d time=%u\\n",
                    currentProcessedActorIdx,
                    player->roomX, player->roomY, player->roomZ,
                    dz, wall->zv.ZVZ1, wall->zv.ZVZ2,
                    player->zv.ZVZ1, player->zv.ZVZ2,
                    (unsigned)timer);
                player->roomZ += dz;
                player->worldZ += dz;
                player->zv.ZVZ1 += dz;
                player->zv.ZVZ2 += dz;
                // The room transform is unchanged, as is the physics world.
                fprintf(stderr,
                    "[FITD-BARRELS] RECOVERED actor=%d xyz=%d,%d,%d zvZ=%d,%d time=%u\\n",
                    currentProcessedActorIdx, player->roomX,
                    player->roomY, player->roomZ,
                    player->zv.ZVZ1, player->zv.ZVZ2,
                    (unsigned)timer);
                fflush(stderr);
            }
        }
    }
#endif
#ifdef FITD_R36S
    // Entered from the lower secret passage: stage 4 / room 0.'''
if s.count(old) != 1:
    raise RuntimeError(f"expected one patched GereDec diagnostic anchor; found {s.count(old)}")
p.write_text(s.replace(old, new, 1))
print("FITD-R36S: geometrically bounded cellar wall-overlap recovery installed")
