#!/usr/bin/env python3
"""Safe A/B isolation of R36S rendering regressions after female-new-game test.

The prior port unconditionally backported ReHaunted mask Z write/test state
without porting its full HD/mask pipeline. That can change scene occlusion.
Restore FITD's ORIGINAL SD mask depth state by default; keep opt-in
  FITD_R36S_MASK_DEPTH=rehaunted
for reproduction without recompilation.

Separately offer ReHaunted's near-camera polygon-sanity test as an OPT-IN:
  FITD_R36S_NEAR_CLIP=on
The default "off" keeps original model geometry. Logs record selected modes.
All changes are isolated from game scripts, saves, the cellar wall fix,
and the optional readable font.
"""
from pathlib import Path
import sys
if len(sys.argv) != 2:
    raise SystemExit("usage: patch_graphics_regression.py UPSTREAM_SOURCE_DIR")
root=Path(sys.argv[1])

def patch(file, name, old, new):
    p = root / file
    s = p.read_text()
    n=s.count(old)
    if n != 1:
        raise RuntimeError(f"{file} {name}: expected 1 occurrence, found {n}")
    p.write_text(s.replace(old,new,1))
    print(f"[FITD-GFX] patched {file}: {name}")

patch("FitdLib/rendererBGFX.cpp","include getenv",
'#include "common.h"',
'''#include "common.h"
#ifdef FITD_R36S
#include <cstdio>
#include <cstdlib>
#include <cstring>
#endif''')

patch("FitdLib/rendererBGFX.cpp","mask A/B compare after ReHaunted mask patch",
'''    // ReHaunted SD mask path: foreground overlay always wins over previously
    // rendered character geometry, and writes depth for subsequent draws.
    // Intentionally no HD alpha blending, postprocessing or new assets.
    bgfx::setState(0 | BGFX_STATE_WRITE_RGB
        | BGFX_STATE_WRITE_Z
        | BGFX_STATE_DEPTH_TEST_ALWAYS
        | BGFX_STATE_MSAA
        | BGFX_STATE_PT_TRISTRIP
    );''',
r'''#ifdef FITD_R36S
    // Stage-only mask: old engine's classical SD occlusion behavior.
    // The ReHaunted Z mode depends on newer camera/depth and mask rendering.
    // Make it opt-in so users can compare on the exact same save.
    static const bool useRehauntedMaskDepth = []() {
        const char* mode = std::getenv("FITD_R36S_MASK_DEPTH");
        const bool enabled = mode && std::strcmp(mode, "rehaunted") == 0;
        fprintf(stderr,
            "[FITD-GFX] MASK_DEPTH=%s (FITD_R36S_MASK_DEPTH=rehaunted for A/B)\n",
            enabled ? "rehaunted" : "classic");
        return enabled;
    }();
    if (useRehauntedMaskDepth) {
        bgfx::setState(BGFX_STATE_WRITE_RGB
            | BGFX_STATE_WRITE_Z
            | BGFX_STATE_DEPTH_TEST_ALWAYS
            | BGFX_STATE_MSAA
            | BGFX_STATE_PT_TRISTRIP);
    } else {
        bgfx::setState(BGFX_STATE_WRITE_RGB
            | BGFX_STATE_MSAA
            | BGFX_STATE_PT_TRISTRIP);
    }
#else
    bgfx::setState(BGFX_STATE_WRITE_RGB
        | BGFX_STATE_WRITE_Z
        | BGFX_STATE_DEPTH_TEST_ALWAYS
        | BGFX_STATE_MSAA
        | BGFX_STATE_PT_TRISTRIP);
#endif''')

patch("FitdLib/renderer.cpp","add near-plane mode selector",
'#include "common.h"',
r'''#include "common.h"
#ifdef FITD_R36S
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ReHaunted detects stretched "floating" polygons near the camera.
// This conservative implementation is opt-in, separate from mask depth.
static bool r36sNearClipEnabled()
{
    static const bool enabled = []() {
        const char* mode = std::getenv("FITD_R36S_NEAR_CLIP");
        const bool selected = mode && (std::strcmp(mode, "on") == 0 ||
                                       std::strcmp(mode, "1") == 0);
        fprintf(stderr, "[FITD-GFX] NEAR_CLIP=%s\n",
                selected ? "on" : "off");
        return selected;
    }();
    return enabled;
}
#endif''')

# Use a unique anchor in processPrim_Poly; the same depth branch occurs in
# processPrim_Point and Sphere so isolate by extracting this function.
p=root/"FitdLib/renderer.cpp"
s=p.read_text()
head='void processPrim_Poly(int primType, sPrimitive* ptr, char** out)'
tail='void processPrim_Point(primTypeEnum primType, sPrimitive* ptr, char** out)'
a=s.find(head);b=s.find(tail,a)
if a<0 or b<0 or b<=a: raise RuntimeError("poly function not found uniquely")
func=s[a:b]
old='''    {
        positionInPrimEntry++;

        numOfPrimitiveToRender++;
        ASSERT(positionInPrimEntry < NUM_MAX_PRIM_ENTRY);
    }
}'''
new=r'''    {
#ifdef FITD_R36S
        // ReHaunted near-camera safety check, no HD texture atlas required.
        // Keep original polygons unless explicitly enabled in graphics.conf.
        bool reject = false;
        if (r36sNearClipEnabled() && g_gameId == AITD1 && depth < 300.f) {
            float maxZ = depth;
            float minSX = 1e30f, maxSX = -1e30f;
            float minSY = 1e30f, maxSY = -1e30f;
            for (int vi = 0; vi < pCurrentPrimEntry->numOfVertices; ++vi) {
                const float sx = pCurrentPrimEntry->vertices[vi].X;
                const float sy = pCurrentPrimEntry->vertices[vi].Y;
                const float sz = pCurrentPrimEntry->vertices[vi].Z;
                if (sx <= -9999.f && sy <= -9999.f && sz <= -9999.f) {
                    reject = true;
                    break;
                }
                if (sx < minSX) minSX = sx;
                if (sx > maxSX) maxSX = sx;
                if (sy < minSY) minSY = sy;
                if (sy > maxSY) maxSY = sy;
                if (sz > maxZ) maxZ = sz;
            }
            if (!reject && (maxZ - depth > 200.f ||
                           maxSX - minSX > 160.f ||
                           maxSY - minSY > 100.f))
                reject = true;
            if (reject) {
                static unsigned badNearPrims = 0;
                ++badNearPrims;
                if (badNearPrims <= 10 || badNearPrims % 250 == 0) {
                    fprintf(stderr,
                        "[FITD-GFX] NEAR_PRIMITIVE_REJECT minZ=%.1f maxZ=%.1f screen=%.1fx%.1f count=%u\n",
                        depth, maxZ, maxSX-minSX, maxSY-minSY, badNearPrims);
                }
            }
        }
        if (!reject)
#endif
        {
            positionInPrimEntry++;

            numOfPrimitiveToRender++;
            ASSERT(positionInPrimEntry < NUM_MAX_PRIM_ENTRY);
        }
    }
}'''
if func.count(old)!=1: raise RuntimeError(f"poly commit anchor found {func.count(old)}")
s=s[:a]+func.replace(old,new,1)+s[b:]
p.write_text(s)
print("[FITD-GFX] installed optional near-camera polygon guard")
