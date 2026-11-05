/*
 * Synthetic camera/room ZV regression; no proprietary AITD game data.
 * Tests real-ETAGE style polygon covers, automatic camera transitions,
 * original mask test-rectangle containment, projection and hard clipping.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "../source/fitd_scene.h"
#include "../source/fitd_stage.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
static void le16(uint8_t *p,int x){
    unsigned u=(unsigned)(uint16_t)x;
    p[0]=(uint8_t)u;p[1]=(uint8_t)(u>>8);
}
static void le32(uint8_t *p,unsigned x){
    le16(p,(int)(x&0xffffu));le16(p+2,(int)(x>>16));
}
static void build_camera(uint8_t *cam,size_t offset,
                         int minx,int maxx,int camera_x){
    uint8_t *p=cam+offset;
    le16(p+6,camera_x);
    le16(p+12,1000);le16(p+14,200);le16(p+16,200);
    le16(p+18,1); /* one viewed room */
    le16(p+20,0); /* room0 */
    le16(p+22,40); /* mask at record+40 */
    le16(p+24,84); /* camera-cover polygon at record+84 */
    uint8_t *mask=p+40;
    le16(mask,1); /* one mask */
    le16(mask+2,1); /* one rect test */
    le16(mask+4,14); /* polygon offset */
    le16(mask+6,-50);le16(mask+8,-50);
    le16(mask+10,50);le16(mask+12,50);
    le16(mask+14,1);le16(mask+16,4);
    const int16_t screen[8]={10,10,20,10,20,20,10,20};
    for(unsigned i=0;i<8;i++)
        le16(mask+18+i*2,screen[i]);
    uint8_t *cover=p+84;
    le16(cover,1);le16(cover+2,4);
    const int16_t world[8]={
        (int16_t)minx,-100,(int16_t)maxx,-100,
        (int16_t)maxx,100,(int16_t)minx,100};
    for(unsigned i=0;i<8;i++)
        le16(cover+4+i*2,world[i]);
}
int main(void){
    int problems=0;
    uint8_t *rooms=(uint8_t*)calloc(1,64);
    uint8_t *cams=(uint8_t*)calloc(1,250);
    if(!rooms || !cams){free(rooms);free(cams);return 3;}
    le32(rooms,4);
    le16(rooms+4,20);le16(rooms+6,22);
    le16(rooms+14,2); /* two cameras */
    le16(rooms+16,0);le16(rooms+18,1);
    le16(rooms+24,0);le16(rooms+26,0);
    le32(cams,8);le32(cams+4,120);
    build_camera(cams,8,-100,0,0);
    build_camera(cams,120,0,100,0);
    FitdStage stage={0};
    const int rc=fitd_stage_set_blobs(&stage,rooms,28,cams,244);
    if(rc!=0){printf("stage init=%d\n",rc);free(rooms);free(cams);return 4;}
    FitdStageRoom room={0};
    FitdStageCamera cam={0};
    if(fitd_stage_room(&stage,0,&room)!=0 || room.camera_count!=2 ||
       fitd_stage_camera(&stage,0,&cam)!=0)problems++;
    if(!fitd_stage_camera_covers(&stage,0,0,-500,0) ||
       fitd_stage_camera_covers(&stage,1,0,-500,0) ||
       !fitd_stage_camera_covers(&stage,1,0,500,0))problems++;
    if(fitd_stage_camera_auto(&stage,&room,-500,0,1)!=0 ||
       fitd_stage_camera_auto(&stage,&room,500,0,0)!=1 ||
       fitd_stage_camera_auto(&stage,&room,-500,0,0)!=0)problems++;
    FitdStageMask *mask=(FitdStageMask*)calloc(1,sizeof(FitdStageMask));
    if(!mask){fitd_stage_release(&stage);return 5;}
    unsigned active=1000;
    int mr=fitd_stage_masks_actor(&stage,0,0,-100,100,-100,100,
                                  mask,NULL,&active);
    if(mr!=0 || active!=1 || !mask->valid ||
       mask->covered_pixels<40)problems++;
    mr=fitd_stage_masks_actor(&stage,0,0,600,800,600,800,
                              mask,NULL,&active);
    if(mr!=0 || active!=0 || mask->covered_pixels!=0 ||
       mask->polygons!=0)problems++;
    FitdSceneActor actor={0};
    FitdSceneScreenPoint p={0};
    if(!fitd_scene_project(&cam,&room,&actor,100,50,0,&p) ||
       fabsf(p.x-180.f)>.01f || fabsf(p.y-110.f)>.01f)
        problems++;
    room.hard_count=1;
    room.hard[0].x1=200;room.hard[0].x2=350;
    room.hard[0].y1=-1000;room.hard[0].y2=1000;
    room.hard[0].z1=-500;room.hard[0].z2=500;
    if(fitd_scene_move(&actor,&room,200,0) ||
       actor.collisions!=1 || actor.x!=0)problems++;
    if(!fitd_scene_move(&actor,&room,-150,0) ||
       actor.steps!=1 || actor.x!=-150.f)problems++;
    free(mask);fitd_stage_release(&stage);
    if(problems){printf("Wii Test06 world camera/actor/mask FAIL count=%d\n",problems);return 1;}
    puts("FITD Wii Test06 world projection, camera cover polygons, actor mask predicates, hard collisions: PASS");
    return 0;
}
