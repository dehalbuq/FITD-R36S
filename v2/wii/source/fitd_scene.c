/*
 * Wii native AITD1 room actor and authentic fixed-camera projection.
 * Camera translation, rotation order Y/X/Z and focal math derived from
 * original Free in the Dark (yaz0r/FITD) InitView / AnimNuage.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "fitd_scene.h"
#include <math.h>
#include <string.h>
#include <limits.h>
#define FITD_ANG (6.2831853071795864769f/1024.f)
static float cs(int16_t a){return cosf((float)a*FITD_ANG);}
static float sn(int16_t a){return sinf((float)a*FITD_ANG);}
bool fitd_scene_project(const FitdStageCamera *camera,
                        const FitdStageRoom *room,
                        const FitdSceneActor *actor,
                        float bx,float by,float bz,
                        FitdSceneScreenPoint *out){
    if(!out)return false;
    memset(out,0,sizeof(*out));
    if(!camera || !camera->valid || !room || !room->valid || !actor)
        return false;
    /* Model local Y rotation, preserving original game world unit scale. */
    const float yaw=(float)(actor->yaw&1023u)*FITD_ANG;
    const float ax=cosf(yaw)*bx-sinf(yaw)*bz;
    const float az=sinf(yaw)*bx+cosf(yaw)*bz;
    /* Original InitView:
       x=(cam.x-room.worldX)*10, y=(room.worldY-cam.y)*10,
       z=(room.worldZ-cam.z)*10; see AnimNuage renderX/Y/Z. */
    float x=actor->x+ax-(float)(camera->x-room->world_x)*10.f;
    float y=actor->y+by-(float)(room->world_y-camera->y)*10.f;
    float z=actor->z+az-(float)(room->world_z-camera->z)*10.f;
    /* Rotate Y, then X, then Z, as the original FITD transformPoint.
       The engine stores full revolutions as 1024 integer angle units. */
    if(camera->beta){
        const float a=cs(camera->beta),b=sn(camera->beta);
        const float u=a*x-b*z;
        z=b*x+a*z;x=u;
    }
    if(camera->alpha){
        const float a=cs(camera->alpha),b=sn(camera->alpha);
        const float u=a*y-b*z;
        z=b*y+a*z;y=u;
    }
    if(camera->gamma){
        const float a=cs(camera->gamma),b=sn(camera->gamma);
        const float u=a*x-b*y;
        y=b*x+a*y;x=u;
    }
    const float depth=z+(float)camera->focal[0];
    out->depth=depth;
    if(depth<=50.f || !isfinite(depth))return false;
    out->x=160.f+x*(float)camera->focal[1]/depth;
    out->y=100.f+y*(float)camera->focal[2]/depth;
    out->visible=isfinite(out->x) && isfinite(out->y) &&
                 fabsf(out->x)<10000.f && fabsf(out->y)<10000.f;
    return out->visible;
}
void fitd_scene_bounds(const FitdSceneActor *actor,
                       int *x1,int *x2,int *z1,int *z2){
    if(!actor)return;
    /* Temporary 200x200 room unit ZV for the Test06 controllable actor.
       Replaced later by each object's own ZV from the real engine. */
    const int x=(int)actor->x;
    const int z=(int)actor->z;
    if(x1)*x1=x-100;
    if(x2)*x2=x+100;
    if(z1)*z1=z-100;
    if(z2)*z2=z+100;
}
bool fitd_scene_move(FitdSceneActor *actor,const FitdStageRoom *room,
                     float dx,float dz){
    if(!actor || !room || !room->valid ||
       !isfinite(dx) || !isfinite(dz))return false;
    if(fabsf(dx)>400.f || fabsf(dz)>400.f)return false;
    const float nx=actor->x+dx,nz=actor->z+dz;
    if(fabsf(nx)>30000.f || fabsf(nz)>30000.f)return false;
    unsigned col=0;
    /* Body physical Y range is a stand-in; a real actor ZV will be
       available once the object and collision system is ported. */
    if(fitd_stage_collision_box(room,(int)nx-100,(int)nx+100,
                                (int)actor->y-300,(int)actor->y+700,
                                (int)nz-100,(int)nz+100,&col)){
        actor->collisions++;
        return false;
    }
    actor->x=nx;actor->z=nz;
    actor->steps++;
    return true;
}
void fitd_scene_choose_spawn(FitdSceneActor *actor,const FitdStage *stage,
                             const FitdStageRoom *room,
                             unsigned camera_id,FILE *log){
    if(!actor)return;
    memset(actor,0,sizeof(*actor));
    if(!stage || !room || !room->valid)return;
    FitdStageCamera cam={0};
    if(fitd_stage_camera(stage,camera_id,&cam)!=0)return;
    /* Prefer the original room origin if it falls in an active camera
       cover polygon and is not inside solid geometry. */
    bool found=false;
    float score=1e20f;
    for(int zi=-24;zi<=24;zi++){
        for(int xi=-24;xi<=24;xi++){
            const int x=xi*250,z=zi*250;
            if(!fitd_stage_camera_covers(stage,camera_id,room->index,x,z))
                continue;
            FitdSceneActor candidate={0};
            candidate.x=(float)x;
            candidate.z=(float)z;
            if(fitd_stage_collision_box(room,x-100,x+100,-300,700,
                                        z-100,z+100,NULL))
                continue;
            FitdSceneScreenPoint origin;
            if(!fitd_scene_project(&cam,room,&candidate,0,400,0,&origin) ||
               origin.x<35 || origin.x>285 ||
               origin.y<25 || origin.y>190)
                continue;
            const float dist=(origin.x-160.f)*(origin.x-160.f)+
                             (origin.y-112.f)*(origin.y-112.f)+
                             (float)(xi*xi+zi*zi)*0.5f;
            if(dist<score){
                score=dist;actor->x=(float)x;actor->z=(float)z;
                found=true;
            }
        }
    }
    if(!found){
        /* Camera cover polygons may not describe the origin.
           Keep a deterministic zero-position fallback, not a random
           spawn invented to match an original save game. */
        actor->x=0;actor->z=0;
    }
    if(log){
        fprintf(log,"FITD-WII-ACTOR06 spawn=%.0f,%.0f,%.0f "
                "camera=%u polygon_match=%d origin_guess=%d\n",
                actor->x,actor->y,actor->z,camera_id,found?1:0,found?0:1);
        fflush(log);
    }
}
