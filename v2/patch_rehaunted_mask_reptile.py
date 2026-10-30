#!/usr/bin/env python3
"""R36S: restore original AITD1 water masks, test ReHaunted SD mask state,
and diagnose reptile world object 228 lifecycle without force-spawning it.

Compare:
- yaz0r/FITD: rendererBGFX.cpp, main.cpp
- spacefarergames/AloneInTheDarkReHaunted:
  TatouSource/FitdLib/rendererBGFX.cpp, main.cpp
The SD mask state is adapted from ReHaunted; we preserve the old R8U texture
with its matching unsigned mask shader. No HD rendering is imported.
"""
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: patch_rehaunted_mask_reptile.py UPSTREAM_SOURCE_DIR")
root = Path(sys.argv[1])

def edit(path, chunks):
    p = root / path
    s = p.read_text()
    for title, old, new in chunks:
        n = s.count(old)
        if n != 1:
            raise RuntimeError(f"{path}: {title}: expected one anchor, got {n}")
        s = s.replace(old, new, 1)
    p.write_text(s)
    print(f"[FITD-REPTILE] patched {path}")

edit("FitdLib/rendererBGFX.cpp", [
    ("correct mask sampler name to match actual compiled maskBackground_ps.sc",
     '''maskTextureUniform = bgfx::createUniform("s_maskTexture", bgfx::UniformType::Sampler);''',
     '''// The SD mask shader declares USAMPLER2D(s_maskPaletteTexture, 0).
        // Keep R8U (integer texture) and bind to the shader's actual sampler.
        maskTextureUniform = bgfx::createUniform("s_maskPaletteTexture", bgfx::UniformType::Sampler);'''),
    ("use ReHaunted's SD foreground mask depth state",
     '''    bgfx::setState(0 | BGFX_STATE_WRITE_RGB
        | BGFX_STATE_MSAA
        | BGFX_STATE_PT_TRISTRIP
    );

    bgfx::setVertexBuffer(0, maskTextures[roomId][maskId].vertexBuffer);''',
     '''    // ReHaunted SD mask path: foreground overlay always wins over previously
    // rendered character geometry, and writes depth for subsequent draws.
    // Intentionally no HD alpha blending, postprocessing or new assets.
    bgfx::setState(0 | BGFX_STATE_WRITE_RGB
        | BGFX_STATE_WRITE_Z
        | BGFX_STATE_DEPTH_TEST_ALWAYS
        | BGFX_STATE_MSAA
        | BGFX_STATE_PT_TRISTRIP
    );

    bgfx::setVertexBuffer(0, maskTextures[roomId][maskId].vertexBuffer);'''),
])

edit("FitdLib/main.cpp", [
    ("trace reptile world state even when object is not an active actor",
     '''void GenereActiveList()
{
\tif(g_gameId > JACK)''',
     '''void GenereActiveList()
{
#ifdef FITD_R36S
    if (g_gameId == AITD1 && g_currentFloor == 5 && currentRoom == 7 &&
        ListWorldObjets.size() > 228) {
        static unsigned lastReptileTrace = 0;
        const unsigned tick = (unsigned)timer;
        if (!lastReptileTrace || tick < lastReptileTrace ||
            tick - lastReptileTrace >= 120) {
            lastReptileTrace = tick;
            const tWorldObject& reptile = ListWorldObjets[228];
            const int actor = reptile.objIndex;
            fprintf(stderr,
                "[FITD-REPTILE] WORLD228 floor=%d room=%d stage=%d objRoom=%d actor=%d life=%d lifeMode=%d body=%d flags=0x%x anim=%d trackMode=%d trackNum=%d time=%u\\n",
                g_currentFloor, currentRoom, reptile.stage, reptile.room,
                actor, reptile.life, reptile.lifeMode, reptile.body,
                (unsigned)reptile.flags, reptile.anim,
                reptile.trackMode, reptile.trackNumber, tick);
            if (actor >= 0 && actor < NUM_MAX_OBJECT) {
                const tObject& active = ListObjets[actor];
                fprintf(stderr,
                    "[FITD-REPTILE] ACTOR228 id=%d world=%d stage=%d room=%d life=%d mode=%d body=%d anim=%d xyz=%d,%d,%d time=%u\\n",
                    actor, active.indexInWorld, active.stage, active.room,
                    active.life, active.lifeMode, active.bodyNum,
                    active.ANIM, active.roomX, active.roomY, active.roomZ, tick);
            }
            fflush(stderr);
        }
    }
#endif
\tif(g_gameId > JACK)'''),
    ("trace strict mask draw status without changing overlay region",
     '''\t\t\tif(isBgOverlayRequired( actorPtr->zv.ZVX1 / 10, actorPtr->zv.ZVX2 / 10,
\t\t\t\tactorPtr->zv.ZVZ1 / 10, actorPtr->zv.ZVZ2 / 10,
\t\t\t\tdata+4,
\t\t\t\t*(s16*)(data) ))
\t\t\t{
\t\t\t\tosystem_setClip(clipLeft, clipTop, clipRight, clipBottom);''',
     '''            const bool maskAllowed = isBgOverlayRequired(
                actorPtr->zv.ZVX1 / 10, actorPtr->zv.ZVX2 / 10,
                actorPtr->zv.ZVZ1 / 10, actorPtr->zv.ZVZ2 / 10,
                data+4, *(s16*)(data));
#ifdef FITD_R36S
            if (g_gameId == AITD1 && g_currentFloor == 5 &&
                currentRoom == 7 && actorPtr->indexInWorld == 1) {
                static unsigned waterMaskCheck = 0;
                ++waterMaskCheck;
                if (waterMaskCheck <= 28 || waterMaskCheck % 600u == 0u) {
                    fprintf(stderr,
                        "[FITD-REPTILE] WATER_MASK cam=%d viewedRoom=%d mask=%d zones=%d draw=%d playerXZ=%d,%d,%d,%d playerY=%d,%d time=%u\\n",
                        NumCamera, relativeCameraIndex, i, *(s16*)data,
                        maskAllowed ? 1 : 0,
                        actorPtr->zv.ZVX1, actorPtr->zv.ZVX2,
                        actorPtr->zv.ZVZ1, actorPtr->zv.ZVZ2,
                        actorPtr->roomY, actorPtr->worldY, (unsigned)timer);
                    fflush(stderr);
                }
            }
#endif
\t\t\tif(maskAllowed)
\t\t\t{
\t\t\t\tosystem_setClip(clipLeft, clipTop, clipRight, clipBottom);'''),
    ("report actual drawn reptile actor",
     '''\t\t\t\tNbAffObjets++;
\t\t\t}
\t\t}
\t}
}

void InitView()''',
     '''#ifdef FITD_R36S
                if (g_gameId == AITD1 && g_currentFloor == 5 &&
                    currentRoom == 7 && actorPtr->indexInWorld == 228) {
                    static unsigned reptileDrawCount = 0;
                    ++reptileDrawCount;
                    if (reptileDrawCount <= 25 || reptileDrawCount % 600u == 0u) {
                        fprintf(stderr,
                            "[FITD-REPTILE] DRAW228 actor=%d body=%d room=%d type=0x%x cam=%d count=%u time=%u\\n",
                            i, actorPtr->bodyNum, actorPtr->room,
                            (unsigned)actorPtr->objectType, NumCamera,
                            reptileDrawCount, (unsigned)timer);
                        fflush(stderr);
                    }
                }
#endif
\t\t\t\tNbAffObjets++;
\t\t\t}
\t\t}
\t}
}

void InitView()''')
])
print("ReHaunted SD mask state and strictly observational reptile debug ready")
