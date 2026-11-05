/*
 * Test06 fixed CAMERA00 cinematic actor in real original room coordinates.
 * The 3D geometry is taken from the user's LISTBODY.PAK, and the
 * camera/FOV from ETAGE00.PAK. No proprietary game assets distributed.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "fitd_scene_gx.h"
#include "fitd_camera.h"
#include <gccore.h>
#include <string.h>
static FitdSceneScreenPoint proj[FITD_BODY_MAX_VERTICES];
static void color(uint8_t index){
    uint8_t rgb[3];
    fitd_camera_palette_rgb(index,rgb);
    GX_Color3f32(rgb[0]/255.f,rgb[1]/255.f,rgb[2]/255.f);
}
static void point(const FitdSceneScreenPoint *p,unsigned w,unsigned h){
    GX_Position3f32(p->x*(float)w/320.f,p->y*(float)h/200.f,0.f);
}
void fitd_scene_gx_draw(const FitdBody *b,const FitdPoseVertex *pose,
                        const FitdStageRoom *room,
                        const FitdStageCamera *camera,
                        const FitdSceneActor *actor,
                        unsigned width,unsigned height,
                        unsigned *projected_vertices){
    if(projected_vertices)*projected_vertices=0;
    if(!b || !b->valid || !camera || !camera->valid ||
       !room || !room->valid || !actor || b->vertex_count>FITD_BODY_MAX_VERTICES)
        return;
    for(unsigned i=0;i<b->vertex_count;i++){
        const float x=pose?pose[i].x:(float)b->vertices[i].x;
        const float y=pose?pose[i].y:(float)b->vertices[i].y;
        const float z=pose?pose[i].z:(float)b->vertices[i].z;
        if(fitd_scene_project(camera,room,actor,x,y,z,&proj[i])){
            if(projected_vertices)(*projected_vertices)++;
        }
    }
    Mtx identity;
    Mtx44 ortho;
    guMtxIdentity(identity);
    guOrtho(ortho,0,(float)height,0,(float)width,0,100);
    GX_LoadPosMtxImm(identity,GX_PNMTX0);
    GX_LoadProjectionMtx(ortho,GX_ORTHOGRAPHIC);
    GX_SetZMode(GX_FALSE,GX_ALWAYS,GX_FALSE);
    GX_SetCullMode(GX_CULL_NONE);
    GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_CLEAR);
    GX_SetNumTevStages(1);
    GX_SetNumChans(1);
    GX_SetNumTexGens(0);
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS,GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0,GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XYZ,GX_F32,0);
    GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_CLR0,GX_CLR_RGBA,GX_RGB8,0);
    GX_SetTevOrder(GX_TEVSTAGE0,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0,GX_PASSCLR);
    for(unsigned i=0;i<b->primitive_count;i++){
        const FitdPrimitive *p=&b->primitives[i];
        bool valid=true;
        for(unsigned j=0;j<p->count;j++){
            if(p->point[j]>=b->vertex_count ||
               !proj[p->point[j]].visible){valid=false;break;}
        }
        if(!valid)continue;
        const unsigned k=p->kind;
        if(k==0){
            GX_Begin(GX_LINES,GX_VTXFMT0,2);
            for(unsigned j=0;j<2;j++){
                point(&proj[p->point[j]],width,height);
                color(p->color);
            }
            GX_End();
        }else if(k==1 || k==8 || k==9 || k==10){
            GX_Begin(GX_TRIANGLEFAN,GX_VTXFMT0,p->count);
            for(unsigned j=0;j<p->count;j++){
                point(&proj[p->point[j]],width,height);
                color(p->color);
            }
            GX_End();
        }else if(k==2 || k==3 || k==6 || k==7){
            GX_SetPointSize(k==3?30:15,GX_TO_ZERO);
            GX_Begin(GX_POINTS,GX_VTXFMT0,1);
            point(&proj[p->point[0]],width,height);
            color(p->color);
            GX_End();
        }
    }
    GX_SetZMode(GX_TRUE,GX_LEQUAL,GX_TRUE);
}
