/*
 * GX RGB5A3 background re-draw through genuine AITD1 camera mask polygons.
 * The Wii consumes tile-ordered 4x4 texture blocks; masked-out texels are
 * transparent. The source image always comes from the user's own game.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef FITD_WII_MASK_GX_H
#define FITD_WII_MASK_GX_H
#include <stdbool.h>
#include <stdint.h>
#include <gccore.h>
#include "fitd_stage.h"
typedef struct {
    GXTexObj texture;
    void *tiled_pixels;
    bool ready;
    unsigned covered_pixels,mask_count,polygon_count;
} FitdMaskOverlay;
bool fitd_mask_gx_init(FitdMaskOverlay *layer);
bool fitd_mask_gx_update(FitdMaskOverlay *layer,
                         const FitdStageMask *mask,
                         const uint8_t *indexed_pixels);
void fitd_mask_gx_draw(FitdMaskOverlay *layer,unsigned width,unsigned height);
void fitd_mask_gx_release(FitdMaskOverlay *layer);
#endif
