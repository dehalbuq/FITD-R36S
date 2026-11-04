/*
 * Original Alone in the Dark BODY preview for Wii GX.
 * Format from Free in the Dark hqr.cpp/createBodyFromPtr (GPLv2).
 * The geometry is decoded bytewise from LE; no native struct overlay.
 *
 * This is a BODY *preview*, not the original animation/physics pipeline.
 * Assets remain only on the user's SD card.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "fitd_body.h"
#include "fitd_pak.h"
#include "fitd_endian.h"
#ifndef FITD_HOST_TEST
#include "fitd_camera.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define INFO_ANIM 2
#define INFO_TORTUE 4
#define INFO_OPTIMISE 8
typedef struct {const uint8_t *b;size_t size,pos;int error;} Reader;
static unsigned read8(Reader *r){
    if(r->error || r->pos>=r->size){r->error=1;return 0;}
    return r->b[r->pos++];
}
static uint16_t read16(Reader *r){
    if(r->error || r->pos>r->size || r->size-r->pos<2){
        r->error=1;return 0;
    }
    uint16_t value=fitd_read_le16(r->b+r->pos);
    r->pos+=2;
    return value;
}
static int16_t read_s16(Reader *r){return (int16_t)read16(r);}
static void skip(Reader *r,size_t n){
    if(r->error || r->pos>r->size || n>r->size-r->pos)
        r->error=1;
    else r->pos+=n;
}
static bool read_index(Reader *r,uint16_t *result,unsigned num_vertices){
    const uint16_t offset=read16(r);
    if(r->error || (offset%6)!=0 || offset/6>=num_vertices){
        r->error=1;return false;
    }
    *result=offset/6;
    return true;
}
int fitd_body_parse(const uint8_t *bytes,size_t size,FitdBody *out){
    if(!bytes || !out || size<22 || size>FITD_PAK_MAX_SIZE)return -1;
    memset(out,0,sizeof(*out));
    Reader r={bytes,size,0,0};
    out->flags=read16(&r);
    if(out->flags & INFO_TORTUE)return -12; /* not implemented upstream */
    skip(&r,12);   /* ZVX1/ZVX2/ZVY1/ZVY2/ZVZ1/ZVZ2 */
    const unsigned scratch=read16(&r);
    if(scratch>size || r.error)return -2;
    skip(&r,scratch);
    const unsigned count=read16(&r);
    if(r.error || count==0 || count>FITD_BODY_MAX_VERTICES)return -3;
    out->vertex_count=(uint16_t)count;
    float vmin[3]={1000000.f,1000000.f,1000000.f};
    float vmax[3]={-1000000.f,-1000000.f,-1000000.f};
    for(unsigned i=0;i<count;i++){
        FitdVertex *v=&out->vertices[i];
        v->x=read_s16(&r);v->y=read_s16(&r);v->z=read_s16(&r);
        if(r.error)return -2;
        const float xyz[3]={(float)v->x,(float)v->y,(float)v->z};
        for(unsigned k=0;k<3;k++){
            if(xyz[k]<vmin[k])vmin[k]=xyz[k];
            if(xyz[k]>vmax[k])vmax[k]=xyz[k];
        }
    }
    float range=0.f;
    for(unsigned k=0;k<3;k++){
        out->center[k]=(vmin[k]+vmax[k])*0.5f;
        const float span=vmax[k]-vmin[k];
        if(span>range)range=span;
    }
    if(range<1.f)range=1.f;
    out->scale=2.65f/range;
    if(out->flags & INFO_ANIM){
        const unsigned groups=read16(&r);
        if(r.error || groups>FITD_BODY_MAX_GROUPS)return -4;
        out->group_count=(uint16_t)groups;
        /* The original BODY stores a byte-offset order table. For this
           static preview we parse and validate all groups without applying
           transforms; no false claim of skeletal animation in Test03. */
        uint16_t order[FITD_BODY_MAX_GROUPS]={0};
        const int group_bytes=(out->flags & INFO_OPTIMISE)?24:16;
        for(unsigned g=0;g<groups;g++){
            order[g]=read16(&r);
            if(r.error || order[g]%(unsigned)group_bytes!=0 ||
               order[g]/(unsigned)group_bytes>=groups)return -5;
        }
        for(unsigned g=0;g<groups;g++){
            FitdGroup *node=&out->groups[g];
            const int16_t start=read_s16(&r);
            node->first=start/6;
            node->count=read_s16(&r);
            const int16_t base=read_s16(&r);
            node->base=base/6;
            node->parent=(int8_t)read8(&r);
            node->group=(int8_t)read8(&r);
            node->kind=read_s16(&r);
            node->dx=read_s16(&r);
            node->dy=read_s16(&r);
            node->dz=read_s16(&r);
            if(out->flags & INFO_OPTIMISE){
                node->rx=read_s16(&r);
                node->ry=read_s16(&r);
                node->rz=read_s16(&r);
                skip(&r,2);
            }
            if(r.error || start<0 || (start%6)!=0 ||
               base<0 || (base%6)!=0 || node->count<0 ||
               (unsigned)node->first>count ||
               (unsigned)node->count>count-(unsigned)node->first)
                return -6;
        }
    }
    const unsigned nprims=read16(&r);
    if(r.error || nprims>FITD_BODY_MAX_PRIMS)return -7;
    out->primitive_count=(uint16_t)nprims;
    for(unsigned i=0;i<nprims;i++){
        FitdPrimitive *p=&out->primitives[i];
        p->kind=(uint8_t)read8(&r);
        if(r.error)return -2;
        unsigned n=0;
        if(p->kind==0){ /* line */
            p->material=(uint8_t)read8(&r);
            p->color=(uint8_t)read8(&r);
            p->extra=(uint8_t)read8(&r);
            n=2;
        }else if(p->kind==1 || p->kind==8 || p->kind==9 || p->kind==10){
            n=read8(&r);  /* vertex count before material/color */
            p->material=(uint8_t)read8(&r);
            p->color=(uint8_t)read8(&r);
            if(n<3 || n>FITD_BODY_MAX_POLY_POINTS)return -8;
        }else if(p->kind==2 || p->kind==6 || p->kind==7 ||
                 p->kind==3){ /* point, big point, zixel, sphere */
            p->material=(uint8_t)read8(&r);
            p->color=(uint8_t)read8(&r);
            p->extra=(uint8_t)read8(&r);
            if(p->kind==3)p->size=read16(&r);
            n=1;
        }else{
            /* Original engine also rejects unsupported Disk/Cylinder. */
            return -9;
        }
        if(r.error)return -2;
        p->count=(uint8_t)n;
        for(unsigned j=0;j<n;j++)
            if(!read_index(&r,&p->point[j],count))return -10;
        if(p->kind==9 || p->kind==10)skip(&r,n*2); /* UV pairs */
        if(r.error)return -2;
    }
    out->valid=true;
    return 0;
}
void fitd_body_release(FitdBody *b){
    if(b)memset(b,0,sizeof(*b));
}
bool fitd_body_load(FitdBody *body,const char *directory,
                    unsigned index,FILE *log){
    if(!body || !directory)return false;
    char filename[256];
    snprintf(filename,sizeof(filename),"%s/LISTBODY.PAK",directory);
    FILE *f=fopen(filename,"rb");
    if(!f){
        if(log){fputs("FITD-WII-BODY archive LISTBODY.PAK not found\n",log);
                fflush(log);}
        return false;
    }
    FitdPakDirectory dir={0};
    FitdPakEntry entry={0};
    int st=fitd_pak_directory(f,&dir);
    if(st==0)st=fitd_pak_entry(f,&dir,index,&entry);
    uint8_t *buf=NULL;
    size_t bytes=0;
    if(st==0)st=fitd_pak_load(f,&entry,&buf,&bytes);
    fclose(f);
    if(st!=0){
        if(log){
            fprintf(log,"FITD-WII-BODY load index=%u/%lu error=%s\n",
                    index,(unsigned long)dir.count,fitd_pak_error(st));
            fflush(log);
        }
        free(buf);
        return false;
    }
    /* Parse into a separate allocation, retaining the displayed body on
       malformed/unsupported records and without overflowing the Wii stack. */
    FitdBody *next=(FitdBody *)calloc(1,sizeof(FitdBody));
    if(!next){free(buf);return false;}
    st=fitd_body_parse(buf,bytes,next);
    free(buf);
    if(st==0){
        next->index=index;
        next->archive_count=dir.count;
        memcpy(body,next,sizeof(FitdBody));
        if(log){
            fprintf(log,"FITD-WII-BODY OK index=%u/%lu flags=%04x "
                    "vertices=%u groups=%u primitives=%u scale=%.5f\n",
                    index,(unsigned long)dir.count,
                    (unsigned)body->flags,
                    (unsigned)body->vertex_count,(unsigned)body->group_count,
                    (unsigned)body->primitive_count,body->scale);
            fflush(log);
        }
    }else if(log){
        fprintf(log,"FITD-WII-BODY REJECT index=%u parser=%d unpacked=%lu\n",
                index,st,(unsigned long)bytes);
        fflush(log);
    }
    free(next);
    return st==0;
}
#ifndef FITD_HOST_TEST
static void color_from_palette(uint8_t index){
    uint8_t rgb[3];
    fitd_camera_palette_rgb(index,rgb);
    GX_Color3f32(rgb[0]/255.f,rgb[1]/255.f,rgb[2]/255.f);
}
static void vertex(const FitdBody *b,uint16_t idx){
    const FitdVertex *v=&b->vertices[idx];
    GX_Position3f32((float)v->x-b->center[0],
                     (float)v->y-b->center[1],
                     (float)v->z-b->center[2]);
}
void fitd_body_draw(const FitdBody *b,Mtx view,Mtx44 projection,
                    float yaw_deg,float px,float pz){
    if(!b || !b->valid || !b->vertex_count)return;
    GX_LoadProjectionMtx(projection,GX_PERSPECTIVE);
    GX_SetZMode(GX_TRUE,GX_LEQUAL,GX_TRUE);
    GX_SetCullMode(GX_CULL_NONE);
    GX_SetNumChans(1);
    GX_SetNumTexGens(0);
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS,GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0,GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XYZ,GX_F32,0);
    GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_CLR0,GX_CLR_RGBA,GX_RGB8,0);
    GX_SetTevOrder(GX_TEVSTAGE0,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0,GX_PASSCLR);
    guVector yaxis={0.f,1.f,0.f};
    Mtx scaled,rotated,model,viewmodel;
    guMtxIdentity(scaled);
    guMtxScaleApply(scaled,scaled,b->scale,b->scale,b->scale);
    guMtxRotAxisDeg(rotated,&yaxis,yaw_deg);
    guMtxConcat(rotated,scaled,model);
    guMtxTransApply(model,model,px,-0.15f,-6.0f+pz);
    guMtxConcat(view,model,viewmodel);
    GX_LoadPosMtxImm(viewmodel,GX_PNMTX0);

    for(unsigned i=0;i<b->primitive_count;i++){
        const FitdPrimitive *p=&b->primitives[i];
        if(p->kind==0){
            GX_Begin(GX_LINES,GX_VTXFMT0,2);
            for(unsigned j=0;j<2;j++){
                vertex(b,p->point[j]);
                color_from_palette(p->color);
            }
            GX_End();
        }else if(p->kind==1 || p->kind==8 || p->kind==9 || p->kind==10){
            /* Classic polygon fan: these are actual decoded BODY polygons.
               Textured body materials 8/9/10 are rendered as flat polygons
               for this stage; texture UVs are decoded but not drawn yet. */
            GX_Begin(GX_TRIANGLEFAN,GX_VTXFMT0,p->count);
            for(unsigned j=0;j<p->count;j++){
                vertex(b,p->point[j]);
                color_from_palette(p->color);
            }
            GX_End();
        }else if(p->kind==2 || p->kind==3 || p->kind==6 || p->kind==7){
            /* Points/spheres approximated by small GX points in Test03. */
            GX_SetPointSize(p->kind==3?32:20,GX_TO_ZERO);
            GX_Begin(GX_POINTS,GX_VTXFMT0,1);
            vertex(b,p->point[0]);
            color_from_palette(p->color);
            GX_End();
        }
    }
}
 #endif /* !FITD_HOST_TEST */
bool fitd_animation_info(FitdAnimInfo *info,const char *directory,
                         unsigned index,FILE *log){
    if(!info || !directory)return false;
    memset(info,0,sizeof(*info));
    char filename[256];
    snprintf(filename,sizeof(filename),"%s/LISTANIM.PAK",directory);
    FILE *f=fopen(filename,"rb");
    if(!f){
        if(log)fprintf(log,"FITD-WII-ANIM LISTANIM.PAK missing\n");
        return false;
    }
    FitdPakDirectory dir={0};
    FitdPakEntry ent={0};
    int st=fitd_pak_directory(f,&dir);
    if(st==0)st=fitd_pak_entry(f,&dir,index,&ent);
    uint8_t *buf=NULL;
    size_t bytes=0;
    if(st==0)st=fitd_pak_load(f,&ent,&buf,&bytes);
    fclose(f);
    if(st==0 && bytes>=12){
        const unsigned frameCount=fitd_read_le16(buf);
        const unsigned groupCount=fitd_read_le16(buf+2);
        const size_t frame8=8u+groupCount*8u;
        const size_t frame16=8u+groupCount*16u;
        if(frameCount>0 && frameCount<=1024 && groupCount<=FITD_BODY_MAX_GROUPS &&
           (bytes==4+frameCount*frame8 ||
            bytes==4+frameCount*frame16)){
            info->frame_count=frameCount;
            info->group_count=groupCount;
            info->first_timestamp=fitd_read_le16(buf+4);
            info->entry_index=index;
            info->valid=true;
        }
    }
    if(log){
        fprintf(log,"FITD-WII-ANIM index=%u status=%s frames=%u groups=%u "
                    "first_tick=%u bytes=%lu\n",
                index,info->valid?"METADATA_OK":"NOT_DECODED",
                info->frame_count,info->group_count,info->first_timestamp,
                (unsigned long)bytes);
        fflush(log);
    }
    free(buf);
    return info->valid;
}
