/*
 * Wii GX compositing pass for original AITD1 mask polygons.
 * RGB5A3 tiled texture: 4x4 blocks, zero = fully transparent,
 * 0x7RGB = opaque indexed-background foreground color.
 * Preview optionally unions mask polygons; true dynamic occlusion needs
 * actor-world-space mask selection from FITD gameplay.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "fitd_mask_gx.h"
#include "fitd_camera.h"
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogc/cache.h>

#define MASK_W FITD_STAGE_MASK_W
#define MASK_H FITD_STAGE_MASK_H

bool fitd_mask_gx_init(FitdMaskOverlay *layer){
    if(!layer)return false;
    memset(layer,0,sizeof(*layer));
    layer->tiled_pixels=memalign(32,MASK_W*MASK_H*2u);
    if(!layer->tiled_pixels)return false;
    memset(layer->tiled_pixels,0,MASK_W*MASK_H*2u);
    DCFlushRange(layer->tiled_pixels,MASK_W*MASK_H*2u);
    GX_InitTexObj(&layer->texture,layer->tiled_pixels,MASK_W,MASK_H,
                  GX_TF_RGB5A3,GX_CLAMP,GX_CLAMP,GX_FALSE);
    return true;
}
bool fitd_mask_gx_update(FitdMaskOverlay *layer,
                         const FitdStageMask *mask,
                         const uint8_t *pixels){
    if(!layer || !layer->tiled_pixels || !mask || !mask->valid || !pixels)return false;
    uint16_t *tile=(uint16_t*)layer->tiled_pixels;
    for(unsigned y=0;y<MASK_H;y+=4){
        for(unsigned x=0;x<MASK_W;x+=4){
            const unsigned tilebase=(y/4u)*(MASK_W/4u)*16u+(x/4u)*16u;
            for(unsigned iy=0;iy<4;iy++)for(unsigned ix=0;ix<4;ix++){
                const unsigned pos=(y+iy)*MASK_W+x+ix;
                uint16_t color=0;
                if(mask->covered[pos]){
                    uint8_t rgb[3];
                    fitd_camera_palette_rgb(pixels[pos],rgb);
                    color=(uint16_t)(0x7000u |
                        ((uint16_t)(rgb[0]>>4)<<8) |
                        ((uint16_t)(rgb[1]>>4)<<4) |
                        (uint16_t)(rgb[2]>>4));
                }
                tile[tilebase+iy*4u+ix]=color;
            }
        }
    }
    DCFlushRange(tile,MASK_W*MASK_H*2u);
    GX_InvalidateTexAll();
    layer->covered_pixels=mask->covered_pixels;
    layer->mask_count=mask->masks;
    layer->polygon_count=mask->polygons;
    layer->ready=mask->covered_pixels>0u;
    return layer->ready;
}
void fitd_mask_gx_draw(FitdMaskOverlay *layer,unsigned width,unsigned height){
    if(!layer || !layer->ready)return;
    Mtx identity;
    Mtx44 ortho;
    guMtxIdentity(identity);
    guOrtho(ortho,0.0f,(float)height,0.0f,(float)width,0.f,100.f);
    GX_LoadPosMtxImm(identity,GX_PNMTX0);
    GX_LoadProjectionMtx(ortho,GX_ORTHOGRAPHIC);
    GX_SetZMode(GX_FALSE,GX_ALWAYS,GX_FALSE);
    GX_SetBlendMode(GX_BM_BLEND,GX_BL_SRCALPHA,GX_BL_INVSRCALPHA,GX_LO_CLEAR);
    GX_SetNumChans(0);
    GX_SetNumTexGens(1);
    GX_SetTexCoordGen(GX_TEXCOORD0,GX_TG_MTX2x4,GX_TG_TEX0,GX_IDENTITY);
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS,GX_DIRECT);
    GX_SetVtxDesc(GX_VA_TEX0,GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XYZ,GX_F32,0);
    GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_TEX0,GX_TEX_ST,GX_F32,0);
    GX_LoadTexObj(&layer->texture,GX_TEXMAP0);
    GX_SetTevOrder(GX_TEVSTAGE0,GX_TEXCOORD0,GX_TEXMAP0,GX_COLORNULL);
    GX_SetTevOp(GX_TEVSTAGE0,GX_REPLACE);
    GX_Begin(GX_QUADS,GX_VTXFMT0,4);
      GX_Position3f32(0,0,0);GX_TexCoord2f32(0,0);
      GX_Position3f32((float)width,0,0);GX_TexCoord2f32(1,0);
      GX_Position3f32((float)width,(float)height,0);GX_TexCoord2f32(1,1);
      GX_Position3f32(0,(float)height,0);GX_TexCoord2f32(0,1);
    GX_End();
    GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_CLEAR);
    GX_SetNumChans(1);
    GX_SetNumTexGens(0);
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS,GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0,GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XYZ,GX_F32,0);
    GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_CLR0,GX_CLR_RGBA,GX_RGB8,0);
    GX_SetTevOrder(GX_TEVSTAGE0,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0,GX_PASSCLR);
    GX_SetZMode(GX_TRUE,GX_LEQUAL,GX_TRUE);
}
void fitd_mask_gx_release(FitdMaskOverlay *layer){
    if(!layer)return;
    free(layer->tiled_pixels);
    memset(layer,0,sizeof(*layer));
}
