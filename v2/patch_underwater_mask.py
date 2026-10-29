#!/usr/bin/env python3
"""Diagnostic and narrowly scoped AITD1 underwater foreground mask correction.

Source base: yaz0r/FITD 543e22bc; compared with
spacefarergames/AloneInTheDarkReHaunted TatouSource/FitdLib/main.cpp.
Only floor 5 / room 7 / original player world object 1 is affected.

The upstream/ReHaunted overlay gate requires the *entire* actor ZV to fit
within one mask's X/Z region. At water boundaries the actor can straddle that
region; try the actor's ZV center only when the original gate rejected it.
Physical Y, collision geometry, saves, life scripts, and other rooms unchanged.
"""
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: patch_underwater_mask.py UPSTREAM_SOURCE_DIR")
p = Path(sys.argv[1]) / "FitdLib/main.cpp"
s = p.read_text()

def replace_once(name, old, new):
    global s
    count = s.count(old)
    if count != 1:
        raise RuntimeError(f"{name}: expected exactly one anchor, got {count}")
    s = s.replace(old, new, 1)

replace_once("water world-vertical trace",
'''void GereSwitchCamera(void)
{
\tint localCurrentCam = NumCamera;''',
'''void GereSwitchCamera(void)
{
#ifdef FITD_R36S
    // Run even if the current camera zone is still valid: the normal routine
    // early-returns while the character moves within the zone.
    if (g_gameId == AITD1 && g_currentFloor == 5 && currentRoom == 7) {
        static unsigned lastWaterTrace = 0;
        if (lastWaterTrace == 0 || (unsigned)timer - lastWaterTrace >= 100u) {
            lastWaterTrace = (unsigned)timer;
            const int playerActor = (currentCameraTargetActor >= 0 &&
                currentCameraTargetActor < NUM_MAX_OBJECT &&
                ListObjets[currentCameraTargetActor].indexInWorld == 1)
                ? currentCameraTargetActor : -1;
            if (playerActor >= 0) {
                const tObject* player = &ListObjets[playerActor];
                fprintf(stderr,
                    "[FITD-WATER] PLAYER room=%d floor=%d actor=%d cam=%d xyz=%d,%d,%d worldY=%d stepY=%d zvY=%d,%d zvXZ=%d,%d,%d,%d ground=%d hardCol=%d hardDec=%d life=%d mode=%d track=%d pos=%d timer=%u\\n",
                    currentRoom, g_currentFloor, playerActor, NumCamera,
                    player->roomX, player->roomY, player->roomZ,
                    player->worldY, player->stepY,
                    player->zv.ZVY1, player->zv.ZVY2,
                    player->zv.ZVX1, player->zv.ZVX2,
                    player->zv.ZVZ1, player->zv.ZVZ2,
                    groundLevel, player->HARD_COL, player->HARD_DEC,
                    player->life, player->lifeMode, player->trackNumber,
                    player->positionInTrack, (unsigned)timer);
                fflush(stderr);
            } else {
                fprintf(stderr, "[FITD-WATER] NO_PLAYER_TARGET target=%d worldTarget=%d timer=%u\\n",
                        currentCameraTargetActor, currentWorldTarget, (unsigned)timer);
            }
        }
    }
#endif
\tint localCurrentCam = NumCamera;''')

replace_once("mask condition, narrow center-based fallback",
'''\t\t\tif(isBgOverlayRequired( actorPtr->zv.ZVX1 / 10, actorPtr->zv.ZVX2 / 10,
\t\t\t\tactorPtr->zv.ZVZ1 / 10, actorPtr->zv.ZVZ2 / 10,
\t\t\t\tdata+4,
\t\t\t\t*(s16*)(data) ))
\t\t\t{
\t\t\t\tosystem_setClip(clipLeft, clipTop, clipRight, clipBottom);
\t\t\t\tosystem_drawMask(relativeCameraIndex, i);''',
'''            bool overlayRequired = isBgOverlayRequired(
                actorPtr->zv.ZVX1 / 10, actorPtr->zv.ZVX2 / 10,
                actorPtr->zv.ZVZ1 / 10, actorPtr->zv.ZVZ2 / 10,
                data + 4, *(s16*)(data));
#ifdef FITD_R36S
            const bool waterPlayer = (g_gameId == AITD1 && g_currentFloor == 5 &&
                currentRoom == 7 && actorPtr->room == 7 &&
                actorPtr->indexInWorld == 1);
            if (waterPlayer) {
                static unsigned waterOverlayTicks = 0;
                static unsigned waterFallbacks = 0;
                const bool sampled = (++waterOverlayTicks <= 24u ||
                                      waterOverlayTicks % 600u == 0u);
                const int zoneCount = *(s16*)data;
                const int centerX = (actorPtr->zv.ZVX1 + actorPtr->zv.ZVX2) / 20;
                const int centerZ = (actorPtr->zv.ZVZ1 + actorPtr->zv.ZVZ2) / 20;
                bool centerInside = false;
                // Only use data that the upstream routine would parse; do not
                // traverse corrupt or negative zone tables.
                if (zoneCount >= 0 && zoneCount <= 64) {
                    const char* zones = data + 4;
                    for (int waterZone = 0; waterZone < zoneCount; ++waterZone) {
                        const int zx1 = *(const s16*)(zones + waterZone*8);
                        const int zz1 = *(const s16*)(zones + waterZone*8 + 2);
                        const int zx2 = *(const s16*)(zones + waterZone*8 + 4);
                        const int zz2 = *(const s16*)(zones + waterZone*8 + 6);
                        if (centerX >= zx1 && centerX <= zx2 &&
                            centerZ >= zz1 && centerZ <= zz2)
                            centerInside = true;
                    }
                }
                if (!overlayRequired && centerInside) {
                    overlayRequired = true;
                    ++waterFallbacks;
                    if (waterFallbacks <= 24 || waterFallbacks % 400u == 0u) {
                        fprintf(stderr,
                            "[FITD-WATER] MASK_FALLBACK cam=%d viewed=%d mask=%d zones=%d playerCenter=%d,%d bboxXZ=%d,%d,%d,%d count=%u timer=%u\\n",
                            NumCamera, relativeCameraIndex, i, zoneCount,
                            centerX, centerZ, actorPtr->zv.ZVX1 / 10,
                            actorPtr->zv.ZVX2 / 10, actorPtr->zv.ZVZ1 / 10,
                            actorPtr->zv.ZVZ2 / 10, waterFallbacks, (unsigned)timer);
                        fflush(stderr);
                    }
                }
                if (sampled) {
                    fprintf(stderr,
                        "[FITD-WATER] MASK_CHECK cam=%d viewed=%d mask=%d zones=%d center=%d,%d strict=%d centerInside=%d draw=%d bboxScreen=%d,%d,%d,%d timer=%u\\n",
                        NumCamera, relativeCameraIndex, i, zoneCount,
                        centerX, centerZ,
                        centerInside ? (overlayRequired && waterFallbacks == 0 ? 1 : 0) : (overlayRequired ? 1 : 0),
                        centerInside ? 1 : 0, overlayRequired ? 1 : 0,
                        BBox3D1, BBox3D2, BBox3D3, BBox3D4, (unsigned)timer);
                    fflush(stderr);
                }
            }
#endif
\t\t\tif(overlayRequired)
\t\t\t{
\t\t\t\tosystem_setClip(clipLeft, clipTop, clipRight, clipBottom);
\t\t\t\tosystem_drawMask(relativeCameraIndex, i);''')

# The strict field must report the untouched upstream test (not cumulative
# fallback counts from prior masks). Keep this diagnostic exact per mask.
old = '''            bool overlayRequired = isBgOverlayRequired(
                actorPtr->zv.ZVX1 / 10, actorPtr->zv.ZVX2 / 10,
                actorPtr->zv.ZVZ1 / 10, actorPtr->zv.ZVZ2 / 10,
                data + 4, *(s16*)(data));'''
new = old + '\n#ifdef FITD_R36S\n            const bool strictOverlayRequired = overlayRequired;\n#endif'
if s.count(old) != 1:
    raise RuntimeError("mask strict logging anchor not unique")
s = s.replace(old, new, 1)
old2 = '''centerInside ? (overlayRequired && waterFallbacks == 0 ? 1 : 0) : (overlayRequired ? 1 : 0),'''
if s.count(old2) != 1:
    raise RuntimeError("strict diagnostic substitution anchor not unique")
s = s.replace(old2, "strictOverlayRequired ? 1 : 0,", 1)

p.write_text(s)
print("FITD-R36S: water-mask center fallback + player height/overlay diagnostics applied")
