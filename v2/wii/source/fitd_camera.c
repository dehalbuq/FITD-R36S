/*
 * Actual index-color camera background -> GX RGB565 tiled texture.
 * Data format based on original yaz0r/FITD main.cpp loadCamera()
 * (320x200 indices) and loadPalette() (ITD_RESS index 3, 256xRGB).
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "fitd_camera.h"
#include "fitd_pak.h"
#include <ogc/system.h>
#include <ogc/cache.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>

#define CAMERA_W 320u
#define CAMERA_H 200u
#define CAMERA_PIXELS (CAMERA_W*CAMERA_H)
static uint8_t paletteRGB[256*3];
static bool paletteReady=false;
void fitd_camera_palette_rgb(uint8_t index,uint8_t rgb[3]){
    if(!rgb)return;
    if(!paletteReady){
        /* Fallback if no original palette was found on SD. */
        rgb[0]=index;rgb[1]=index;rgb[2]=index;
        return;
    }
    rgb[0]=paletteRGB[(unsigned)index*3];
    rgb[1]=paletteRGB[(unsigned)index*3+1];
    rgb[2]=paletteRGB[(unsigned)index*3+2];
}


static void log_info(FILE *log,const char *label,int frame,int status,
                     const FitdPakEntry *e){
    if(!log)return;
    fprintf(log,"FITD-WII-CAMERA %s frame=%d status=%s",label,frame,
            fitd_pak_error(status));
    if(e)fprintf(log," pack=%lu unpack=%lu comp=%u flags=%u",
                 (unsigned long)e->packed_size,
                 (unsigned long)e->unpacked_size,
                 (unsigned)e->compression,(unsigned)e->flags);
    fputc('\n',log);
    fflush(log);
}
static int load_archive_entry(const char *fullpath,unsigned index,
                              uint8_t **out,size_t *len,FILE *log,
                              FitdPakDirectory *directory,
                              FitdPakEntry *entry){
    FILE *f=fopen(fullpath,"rb");
    if(!f)return -2;
    int status=fitd_pak_directory(f,directory);
    if(!status)status=fitd_pak_entry(f,directory,index,entry);
    if(!status)status=fitd_pak_load(f,entry,out,len);
    fclose(f);
    if(status)log_info(log,fullpath,index,status,NULL);
    return status;
}
static void init_grayscale(void){
    for(unsigned i=0;i<256;i++){
        paletteRGB[i*3]=i;
        paletteRGB[i*3+1]=i;
        paletteRGB[i*3+2]=i;
    }
}
/* 6-bit VGA palette expanded to Wii RGB, with auto-detection.
   The original AITD1 ITD_RESS.PAK entry 3 stores 768 RGB bytes. */
static void load_itd_palette(const char *directory,FILE *log){
    char path[256];
    snprintf(path,sizeof(path),"%s/ITD_RESS.PAK",directory);
    uint8_t *data=NULL;
    size_t size=0;
    FitdPakDirectory dir={0};
    FitdPakEntry ent={0};
    init_grayscale();
    paletteReady=false;
    if(load_archive_entry(path,3,&data,&size,log,&dir,&ent)==0 && size>=768){
        unsigned mx=0;
        for(size_t i=0;i<768;i++)if(data[i]>mx)mx=data[i];
        for(unsigned i=0;i<768;i++){
            unsigned val=data[i];
            paletteRGB[i]=(uint8_t)(mx<=63?((val*255u+31u)/63u):val);
        }
        paletteReady=true;
        if(log)fprintf(log,"FITD-WII-PALETTE mode=%s source=ITD_RESS.PAK entry=3 size=%lu\n",
                       mx<=63?"vga6-to-rgb8":"rgb8",
                       (unsigned long)size);
    } else {
        if(log)fprintf(log,"FITD-WII-PALETTE fallback=grayscale reason=missing_or_unsupported\n");
    }
    free(data);
    if(log)fflush(log);
}
static inline uint16_t rgb565(uint8_t r,uint8_t g,uint8_t b){
    return (uint16_t)((((uint16_t)r>>3)<<11) |
                      (((uint16_t)g>>2)<<5) |
                      ((uint16_t)b>>3));
}
/* GX_TF_RGB565 uses 4x4 texel tiles, not linear rows. */
static void upload_palette_indexed(uint16_t *tile,const uint8_t *pixels){
    for(unsigned tileY=0;tileY<CAMERA_H;tileY+=4){
        for(unsigned tileX=0;tileX<CAMERA_W;tileX+=4){
            const unsigned tileStart=
                (tileY/4)*(CAMERA_W/4)*16+(tileX/4)*16;
            for(unsigned y=0;y<4;y++){
                for(unsigned x=0;x<4;x++){
                    const unsigned index=pixels[(tileY+y)*CAMERA_W+tileX+x];
                    const unsigned off=index*3;
                    tile[tileStart+y*4+x]=rgb565(
                        paletteRGB[off],paletteRGB[off+1],paletteRGB[off+2]);
                }
            }
        }
    }
}
bool fitd_camera_set(FitdCamera *c,const char *directory,
                     unsigned frame,FILE *log){
    if(!c || !c->tiled_pixels || !directory)return false;
    char path[256];
    snprintf(path,sizeof(path),"%s/CAMERA00.PAK",directory);
    uint8_t *image=NULL;
    size_t size=0;
    FitdPakDirectory dir={0};
    FitdPakEntry entry={0};
    int status=load_archive_entry(path,frame,&image,&size,log,&dir,&entry);
    if(status || size<CAMERA_PIXELS){
        if(!status)status=-4;
        log_info(log,"CAMERA00_BAD_IMAGE",frame,status,&entry);
        free(image);
        return false;
    }
    upload_palette_indexed((uint16_t*)c->tiled_pixels,image);
    DCFlushRange(c->tiled_pixels,CAMERA_PIXELS*2);
    GX_InitTexObj(&c->tex,c->tiled_pixels,CAMERA_W,CAMERA_H,
                  GX_TF_RGB565,GX_CLAMP,GX_CLAMP,GX_FALSE);
    c->current=frame;
    c->valid=true;
    log_info(log,"CAMERA00_LOADED",frame,0,&entry);
    free(image);
    return true;
}
bool fitd_camera_open(FitdCamera *c,const char *directory,FILE *log){
    if(!c || !directory)return false;
    memset(c,0,sizeof(*c));
    char path[256];
    snprintf(path,sizeof(path),"%s/CAMERA00.PAK",directory);
    FILE *f=fopen(path,"rb");
    if(!f){
        if(log)fprintf(log,"FITD-WII-ARCHIVE CAMERA00.PAK not-found\n");
        return false;
    }
    FitdPakDirectory dir;
    const int status=fitd_pak_directory(f,&dir);
    fclose(f);
    if(status){
        log_info(log,"CAMERA00_DIR",0,status,NULL);
        return false;
    }
    c->frames=dir.count;
    if(log){
        fprintf(log,"FITD-WII-ARCHIVE CAMERA00.PAK entries=%lu bytes=%lu\n",
                (unsigned long)dir.count,(unsigned long)dir.file_size);
        fflush(log);
    }
    if(c->frames==0)return false;
    c->tiled_pixels=memalign(32,CAMERA_PIXELS*2);
    if(!c->tiled_pixels){
        if(log)fprintf(log,"FITD-WII-CAMERA GX_TEXTURE_OOM\n");
        return false;
    }
    load_itd_palette(directory,log);
    c->correct_palette=paletteReady;
    return fitd_camera_set(c,directory,0,log);
}
void fitd_camera_draw(const FitdCamera *c,unsigned width,unsigned height){
    if(!c || !c->valid)return;
    // Use Wii's full 4:3 area for classic AITD 320x200 backgrounds.
    // The actor GX demo remains a separate overlay for the engine milestone.
    Mtx identity;
    Mtx44 ortho;
    guMtxIdentity(identity);
    guOrtho(ortho,0.0f,(float)height,0.0f,(float)width,0.f,100.f);
    GX_LoadPosMtxImm(identity,GX_PNMTX0);
    GX_LoadProjectionMtx(ortho,GX_ORTHOGRAPHIC);
    GX_SetZMode(GX_FALSE,GX_ALWAYS,GX_FALSE);
    GX_SetNumChans(0);
    GX_SetNumTexGens(1);
    GX_SetTexCoordGen(GX_TEXCOORD0,GX_TG_MTX2x4,GX_TG_TEX0,GX_IDENTITY);
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS,GX_DIRECT);
    GX_SetVtxDesc(GX_VA_TEX0,GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XYZ,GX_F32,0);
    GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_TEX0,GX_TEX_ST,GX_F32,0);
    GXTexObj tex=c->tex;
    GX_LoadTexObj(&tex,GX_TEXMAP0);
    GX_SetTevOrder(GX_TEVSTAGE0,GX_TEXCOORD0,GX_TEXMAP0,GX_COLORNULL);
    GX_SetTevOp(GX_TEVSTAGE0,GX_REPLACE);
    GX_Begin(GX_QUADS,GX_VTXFMT0,4);
      GX_Position3f32(0.f,0.f,0.f);GX_TexCoord2f32(0.f,0.f);
      GX_Position3f32((float)width,0.f,0.f);GX_TexCoord2f32(1.f,0.f);
      GX_Position3f32((float)width,(float)height,0.f);GX_TexCoord2f32(1.f,1.f);
      GX_Position3f32(0.f,(float)height,0.f);GX_TexCoord2f32(0.f,1.f);
    GX_End();
    // Restore GX state for solid 3D actors in the prototype scene.
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
void fitd_camera_release(FitdCamera *c){
    if(!c)return;
    free(c->tiled_pixels);
    memset(c,0,sizeof(*c));
}
