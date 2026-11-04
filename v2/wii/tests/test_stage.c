/*
 * Synthetic AITD1 ETAGE room/camera/mask test, no proprietary data.
 * Tests PowerPC-style unaligned little-endian metadata and polygon
 * coverage of masked background pixels; invalid offsets rejected.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "../source/fitd_stage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void w16(uint8_t *p,unsigned n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
static void w32(uint8_t *p,unsigned n){
    p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);
    p[2]=(uint8_t)(n>>16);p[3]=(uint8_t)(n>>24);
}
static int test_valid(void){
    uint8_t *room=(uint8_t*)calloc(1,128);
    uint8_t *cam=(uint8_t*)calloc(1,128);
    if(!room || !cam){free(room);free(cam);return 1;}
    w32(room,4);     /* one entry, room record at +4 */
    w16(room+4,14);  /* hard zones at offset +14 */
    w16(room+6,16);  /* scene zones at +16 */
    w16(room+8,111);
    w16(room+10,222);
    w16(room+12,333);
    w16(room+14,1);  /* 1 camera */
    w16(room+16,0);  /* camera id 0 */
    w16(room+18,0);  /* no hard zones */
    w16(room+20,0);  /* no scene zones */

    w32(cam,4);      /* one camera entry */
    w16(cam+4+0,256); /* angle */
    w16(cam+4+6,1000); /* camera x */
    w16(cam+4+12,350); /* perspective */
    w16(cam+4+14,500); /* focal x */
    w16(cam+4+16,500); /* focal y */
    w16(cam+4+18,1);  /* viewed rooms */
    w16(cam+4+20,0);  /* room 0 */
    w16(cam+4+22,32); /* mask base at +32 */
    w16(cam+4+24,0);  /* no covers */
    /* mask base relative to camera record */
    const unsigned base=4+32;
    w16(cam+base,1);  /* 1 occlusion mask */
    w16(cam+base+2,1); /* one XZ mask test rect */
    w16(cam+base+4,14); /* polygon block relative to mask base */
    w16(cam+base+6,0);w16(cam+base+8,0);
    w16(cam+base+10,100);w16(cam+base+12,100);
    w16(cam+base+14,1); /* 1 polygon */
    w16(cam+base+16,4); /* 4 points */
    const int16_t square[8]={10,10,20,10,20,20,10,20};
    for(int j=0;j<8;j++)
        w16(cam+base+18+j*2,(uint16_t)square[j]);
    FitdStage stage={0};
    if(fitd_stage_set_blobs(&stage,room,22,cam,base+34)!=0){
        free(room);free(cam);return 2;
    }
    FitdStageRoom data;
    FitdStageCamera camera;
    FitdStageMask *mask=(FitdStageMask*)calloc(1,sizeof(*mask));
    if(!mask){fitd_stage_release(&stage);return 3;}
    int a=fitd_stage_room(&stage,0,&data);
    int b=fitd_stage_camera(&stage,0,&camera);
    int c=fitd_stage_masks(&stage,0,0,mask,NULL);
    int pass=a==0 && b==0 && c==0 && data.valid &&
             data.world_x==111 && data.world_y==222 &&
             data.camera_count==1 && data.camera_ids[0]==0 &&
             camera.alpha==256 && camera.focal[0]==350 &&
             camera.viewed_count==1 && mask->masks==1 &&
             mask->polygons==1 && mask->covered_pixels>60 &&
             mask->covered[15*320+15]==255 &&
             mask->covered[2*320+2]==0;
    const int wrong=fitd_stage_masks(&stage,0,3,mask,NULL);
    if(wrong>=0)pass=0;
    FitdStageZone h={10,20,-3,3,10,20,0,0};
    data.hard_count=1;data.hard[0]=h;
    if(!fitd_stage_collision_xz(&data,15,0,15,NULL) ||
       fitd_stage_collision_xz(&data,3,0,3,NULL))pass=0;
    cam[4+22]=0xFF;
    if(fitd_stage_masks(&stage,0,0,mask,NULL)>=0)pass=0;
    free(mask);
    fitd_stage_release(&stage);
    if(!pass)printf("STAGE test failed: room=%d camera=%d mask=%d wrong=%d\n",a,b,c,wrong);
    return pass?0:4;
}
/*
 * Real AITD1 ETAGE inner offset directories are not self-counted.
 * The first room record follows an 8-byte offset area: its header
 * overlaps the would-be second directory slot. A camera table with five
 * entries similarly has a sixth slot occupied by the camera header.
 * These bytes are synthesized; no original game bytes are included.
 */
static int test_real_shape(void){
    uint8_t rooms[96]={0};
    uint8_t cameras[240]={0};
    w32(rooms+0,8);        /* valid room offset 0 */
    w32(rooms+4,0x000F0017u); /* first room offset table terminator */
    w32(cameras+0,24);    /* first camera begins at 24 */
    w32(cameras+4,60);
    w32(cameras+8,100);
    w32(cameras+12,140);
    w32(cameras+16,180);
    w32(cameras+20,0xF1230011u); /* camera 0 header, not entry 5 */
    unsigned nr=0,nc=0;
    if(fitd_stage_blob_count(rooms,sizeof(rooms),&nr)!=0 || nr!=1)
        return 1;
    if(fitd_stage_blob_count(cameras,sizeof(cameras),&nc)!=0 || nc!=5)
        return 2;
    uint8_t *r=(uint8_t*)malloc(sizeof(rooms));
    uint8_t *c=(uint8_t*)malloc(sizeof(cameras));
    if(!r || !c){free(r);free(c);return 3;}
    memcpy(r,rooms,sizeof(rooms));memcpy(c,cameras,sizeof(cameras));
    FitdStage stage={0};
    const int status=fitd_stage_set_blobs(&stage,r,sizeof(rooms),c,sizeof(cameras));
    if(status!=0){free(r);free(c);return 4;}
    const int ok=stage.room_count==1 && stage.camera_count==5;
    fitd_stage_release(&stage);
    return ok?0:5;
}
static int test_bad(void){
    uint8_t bytes[12]={0};
    unsigned count=0;
    w32(bytes,14); /* unaligned first offset */
    if(fitd_stage_blob_count(bytes,sizeof(bytes),&count)>=0)return 1;
    w32(bytes,12); /* offset points to exact EOF */
    if(fitd_stage_blob_count(bytes,sizeof(bytes),&count)>=0)return 2;
    w32(bytes,8);w32(bytes+4,4); /* candidate after entry0 is invalid */
    if(fitd_stage_blob_count(bytes,sizeof(bytes),&count)!=0 || count!=1)return 3;
    return 0;
}
int main(void){
    int a=test_valid(),b=test_bad(),c=test_real_shape();
    if(a||b||c){printf("FITD Wii Test05.2 ETAGE tests FAILED: valid=%d bad=%d real_shape=%d\n",a,b,c);return 1;}
    puts("FITD Wii Test05.2 ETAGE offsets: PASS (1 room, 5 cameras, table terminator, mask, collision)");
    return 0;
}
