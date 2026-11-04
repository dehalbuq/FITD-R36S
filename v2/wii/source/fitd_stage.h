/*
 * FITD AITD1 ETAGExx.PAK stage reader for Nintendo Wii PowerPC.
 * Stage archive entry 0 = room data offsets + records;
 * entry 1 = camera data offsets + records.
 * Original format traced to yaz0r/FITD FitdLib/floor.cpp, room.cpp,
 * main.cpp/createAITD1Mask. This port is read-only.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef FITD_WII_STAGE_H
#define FITD_WII_STAGE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#define FITD_STAGE_MAX_ROOM_CAMERAS 64u
#define FITD_STAGE_MAX_ZONE_RECORDS 128u
#define FITD_STAGE_MAX_ROOMS 512u
#define FITD_STAGE_MAX_CAMERAS 512u
#define FITD_STAGE_MASK_W 320u
#define FITD_STAGE_MASK_H 200u
#define FITD_STAGE_MASK_PIXELS (FITD_STAGE_MASK_W*FITD_STAGE_MASK_H)
typedef struct {
    int16_t x1,x2,y1,y2,z1,z2;
    uint16_t parameter,type;
} FitdStageZone;
typedef struct {
    int16_t world_x,world_y,world_z;
    unsigned index,camera_count,hard_count,scene_count;
    uint16_t camera_ids[FITD_STAGE_MAX_ROOM_CAMERAS];
    FitdStageZone hard[FITD_STAGE_MAX_ZONE_RECORDS];
    FitdStageZone scene[FITD_STAGE_MAX_ZONE_RECORDS];
    bool valid;
} FitdStageRoom;
typedef struct {
    unsigned room_index;
    uint16_t mask_offset,cover_offset;
    int16_t light_x,light_y,light_z;
} FitdStageCameraView;
typedef struct {
    int16_t alpha,beta,gamma,x,y,z;
    uint16_t focal[3];
    unsigned index,viewed_count;
    FitdStageCameraView viewed[FITD_STAGE_MAX_ROOM_CAMERAS];
    size_t raw_offset,raw_limit;
    bool valid;
} FitdStageCamera;
typedef struct {
    uint8_t *rooms,*cameras;
    size_t rooms_size,cameras_size;
    unsigned floor_number,room_count,camera_count;
    bool valid;
} FitdStage;
typedef struct {
    uint8_t covered[FITD_STAGE_MASK_PIXELS];
    unsigned masks,polygons,covered_pixels;
    int viewed_room;
    bool valid;
} FitdStageMask;
int fitd_stage_blob_count(const uint8_t *data,size_t len,unsigned *count);
int fitd_stage_set_blobs(FitdStage *stage,uint8_t *room_bytes,
                         size_t room_size,uint8_t *camera_bytes,
                         size_t camera_size);
bool fitd_stage_load(FitdStage *stage,const char *directory,
                     unsigned floor,FILE *log);
void fitd_stage_release(FitdStage *stage);
int fitd_stage_room(const FitdStage *stage,unsigned index,
                    FitdStageRoom *room);
int fitd_stage_camera(const FitdStage *stage,unsigned index,
                      FitdStageCamera *camera);
int fitd_stage_raster_polygon(uint8_t *mask,size_t pixels,
                              const int16_t *xy,unsigned point_count);
/* selected_mask=-1 composites every mask (old Test05 behavior).
   selected_mask>=0 composites only that physical mask; out->masks
   still reports the total original mask count for browsing. */
int fitd_stage_masks_selected(const FitdStage *stage,unsigned camera_index,
                              int room_index,int selected_mask,
                              FitdStageMask *out,FILE *log);
int fitd_stage_masks(const FitdStage *stage,unsigned camera_index,
                     int room_index,FitdStageMask *out,FILE *log);
bool fitd_stage_collision_xz(const FitdStageRoom *room,
                             int x,int y,int z,unsigned *index);
#endif
