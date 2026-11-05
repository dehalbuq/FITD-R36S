/* Wii GX RGBA8 tiled texture addressing, also host-testable.
 * 4x4 tiles store 16 AR pairs, then 16 GB pairs (64 bytes/tile).
 * Keep the camera RGB even in transparent pixels so linear filtering
 * cannot interpolate foreground colors towards black.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef FITD_WII_MASK_TEXTURE_H
#define FITD_WII_MASK_TEXTURE_H
#include <stdint.h>
#include <stddef.h>
static inline size_t fitd_mask_rgba8_offset(unsigned width,unsigned x,unsigned y){
    return (((size_t)(y>>2)*(width>>2)+(x>>2))*64u)+
           ((((size_t)(y&3u)*4u)+(x&3u))*2u);
}
static inline void fitd_mask_rgba8_write(uint8_t *out,unsigned width,
                                          unsigned x,unsigned y,
                                          uint8_t alpha,const uint8_t rgb[3]){
    const size_t i=fitd_mask_rgba8_offset(width,x,y);
    out[i]=alpha;
    out[i+1u]=rgb[0];
    out[i+32u]=rgb[1];
    out[i+33u]=rgb[2];
}
#endif
