/*
 * Wii GX compositing pass for original AITD1 mask polygons.
 * RGBA8 tiled texture: 4x4 blocks, AR/GB split planes. Background
 * RGB is preserved for every texel (including alpha-zero texels),
 * preventing dark halos during linear filtering over mask contours.
 * Preview optionally unions mask polygons; true dynamic occlusion needs
 * actor-world-space mask selection from FITD gameplay.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "fitd_mask_gx.h"
#include "fitd_camera.h"
#include "fitd_mask_texture.h"
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogc/cache.h>

#define MASK_W FITD_STAGE_MASK_W
#define MASK_H FITD_STAGE_MASK_H
#define MASK_TEXTURE_BYTES (MASK_W*MASK_H*4u)

bool fitd_mask_gx_init(FitdMaskOverlay *layer){
    if(!layer)return false;
    memset(layer,0,sizeof(*layer));
    layer->tiled_pixels=memalign(32,MASK_TEXTURE_BYTES);
    if(!layer->tiled_pixels)return false;
    memset(layer->tiled_pixels,0,MASK_TEXTURE_BYTES);
    DCFlushRange(layer->tiled_pixels,MASK_TEXTURE_BYTES);
    GX_InitTexObj(&layer->texture,layer->tiled_pixels,MASK_W,MASK_H,
                  GX_TF_RGBA8,GX_CLAMP,GX_CLAMP,GX_FALSE);
    /* Match the camera's soft bilinear sampling, but without black
       texel bleed: alpha-zero texels carry the exact background RGB. */
    GX_InitTexObjFilterMode(&layer->texture,GX_LINEAR,GX_LINEAR);
    return true;
}
bool fitd_mask_gx_update(FitdMaskOverlay *layer,
                         const FitdStageMask *mask,
                         const uint8_t *pixels){
    if(!layer || !layer->tiled_pixels || !mask || !mask->valid || !pixels)return false;
    uint8_t *tile=(uint8_t*)layer->tiled_pixels;
    for(unsigned y=0;y<MASK_H;y++){
        for(unsigned x=0;x<MASK_W;x++){
            const unsigned pos=y*MASK_W+x;
            uint8_t rgb[3];
            fitd_camera_palette_rgb(pixels[pos],rgb);
            /* When alpha=0, preserve the *same* background RGB. If
               transparent RGB were black, bilinear mask sampling would
               generate the black outline reported in Test06.2. */
            fitd_mask_rgba8_write(tile,MASK_W,x,y,
                mask->covered[pos]?255u:0u,rgb);
        }
    }
    DCFlushRange(tile,MASK_TEXTURE_BYTES);
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
    /* Binary cutout alpha; transparent texels do not touch the EFB. */
    GX_SetAlphaCompare(GX_GREATER,0,GX_AOP_AND,GX_ALWAYS,0);
    GX_SetNumTevStages(1);
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
    GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
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
