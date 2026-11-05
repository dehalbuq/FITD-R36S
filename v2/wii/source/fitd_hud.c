/* Original small seven-segment HUD. Three big digits show the
 * original LISTANIM index; lower bar indicates gallery progress.
 * Drawn after all camera/mask rendering, without external fonts.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "fitd_hud.h"
#include <gccore.h>
static void rect(float x,float y,float w,float h,
                 unsigned char r,unsigned char g,unsigned char b,
                 unsigned char alpha){
    GX_Begin(GX_QUADS,GX_VTXFMT0,4);
      GX_Position3f32(x,y,0);GX_Color4u8(r,g,b,alpha);
      GX_Position3f32(x+w,y,0);GX_Color4u8(r,g,b,alpha);
      GX_Position3f32(x+w,y+h,0);GX_Color4u8(r,g,b,alpha);
      GX_Position3f32(x,y+h,0);GX_Color4u8(r,g,b,alpha);
    GX_End();
}
static void number(unsigned d,float x,float y,
                   unsigned char r,unsigned char g,unsigned char b){
    static const unsigned char segs[10]={0x3f,0x06,0x5b,0x4f,0x66,
                                          0x6d,0x7d,0x07,0x7f,0x6f};
    static const unsigned char seg_xy[7][4]={
        {3,0,15,3},{18,3,3,10},{18,15,3,10},{3,25,15,3},
        {0,15,3,10},{0,3,3,10},{3,12,15,3}
    };
    if(d>9)return;
    for(unsigned i=0;i<7u;i++)if((segs[d]&(1u<<i))!=0u)
        rect(x+seg_xy[i][0],y+seg_xy[i][1],seg_xy[i][2],seg_xy[i][3],
             r,g,b,255u);
}
void fitd_hud_draw(unsigned index,unsigned current,unsigned total,
                   unsigned width,unsigned height,bool autoplay){
    if(!width || !height)return;
    Mtx identity;
    Mtx44 projection;
    guMtxIdentity(identity);
    guOrtho(projection,0.f,(float)height,0.f,(float)width,0.f,100.f);
    GX_LoadPosMtxImm(identity,GX_PNMTX0);
    GX_LoadProjectionMtx(projection,GX_ORTHOGRAPHIC);
    GX_SetZMode(GX_FALSE,GX_ALWAYS,GX_FALSE);
    GX_SetNumTevStages(1);
    GX_SetNumChans(1);
    GX_SetNumTexGens(0);
    GX_SetBlendMode(GX_BM_BLEND,GX_BL_SRCALPHA,GX_BL_INVSRCALPHA,GX_LO_CLEAR);
    GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS,GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0,GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XYZ,GX_F32,0);
    GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_CLR0,GX_CLR_RGBA,GX_RGBA8,0);
    GX_SetTevOrder(GX_TEVSTAGE0,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0,GX_PASSCLR);
    rect(8,9,92,59,12,16,27,208);
    const unsigned d=index%1000u;
    number(d/100u,17,18,autoplay?242:99,autoplay?198:230,105);
    number((d/10u)%10u,44,18,autoplay?242:99,autoplay?198:230,105);
    number(d%10u,71,18,autoplay?242:99,autoplay?198:230,105);
    rect(17,57,74,4,60,66,77,255);
    if(autoplay && total>0){
        if(current>total)current=total;
        rect(17,57,74.f*(float)current/(float)total,4,90,217,121,255);
    }else{
        rect(17,57,74,4,90,130,221,255);
    }
    GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_CLEAR);
    GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_CLR0,GX_CLR_RGBA,GX_RGB8,0);
    GX_SetZMode(GX_TRUE,GX_LEQUAL,GX_TRUE);
}
