/*
 * GX camera image preview: genuine classic AITD1 CAMERA00.PAK background.
 * This is an asset/renderer milestone, not the entire game engine.
 * Original game archives must be supplied separately from the user's copy.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef FITD_WII_CAMERA_H
#define FITD_WII_CAMERA_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <gccore.h>
typedef struct {
    GXTexObj tex;
    void *tiled_pixels;  /* 320*200*2 bytes, 32-byte aligned MEM1/MEM2 */
    uint8_t *raw_pixels; /* 320x200 original indexed colors for mask overlay */
    unsigned frames;
    unsigned current;
    bool valid,correct_palette;
} FitdCamera;
bool fitd_camera_open(FitdCamera *c,const char *directory,FILE *log);
bool fitd_camera_set(FitdCamera *c,const char *directory,
                     unsigned frame,FILE *log);
void fitd_camera_draw(const FitdCamera *c,unsigned width,unsigned height);
/* Original VGA/RGB palette exposed to the GX BODY renderer. */
void fitd_camera_palette_rgb(uint8_t index,uint8_t rgb[3]);
void fitd_camera_release(FitdCamera *c);
#endif
