/*
 * Original AITD BODY native GX preview.
 * Format follows GPL source yaz0r/FITD FitdLib/hqr.cpp createBodyFromPtr().
 * Nintendo Wii PowerPC big-endian: all raw fields are decoded as LE.
 * Prototype parser/renderer; NOT yet full bone animation, scene masking or AI.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef FITD_WII_BODY_H
#define FITD_WII_BODY_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#ifndef FITD_HOST_TEST
#include <gccore.h>
#endif
#define FITD_BODY_MAX_VERTICES 1000
#define FITD_BODY_MAX_GROUPS 64
#define FITD_BODY_MAX_PRIMS 1500
#define FITD_BODY_MAX_POLY_POINTS 32
typedef struct {int16_t x,y,z;} FitdVertex;
typedef struct {float x,y,z;} FitdPoseVertex;
typedef struct {
    int16_t first,count,base;
    int8_t parent,group;
    int16_t kind,dx,dy,dz,rx,ry,rz;
} FitdGroup;
typedef struct {
    uint8_t kind,material,color,extra;
    uint16_t size;
    uint8_t count;
    uint16_t point[FITD_BODY_MAX_POLY_POINTS];
} FitdPrimitive;
typedef struct {
    uint16_t flags,vertex_count,group_count,primitive_count;
    unsigned index,archive_count;
    FitdVertex vertices[FITD_BODY_MAX_VERTICES];
    FitdGroup groups[FITD_BODY_MAX_GROUPS];
    uint16_t group_order[FITD_BODY_MAX_GROUPS];
    FitdPrimitive primitives[FITD_BODY_MAX_PRIMS];
    float center[3],scale;
    uint16_t min_xyz[3],max_xyz[3]; /* for telemetry only */
    bool valid;
} FitdBody;
typedef struct {
    unsigned frame_count,group_count;
    uint16_t first_timestamp;
    unsigned entry_index;
    bool valid;
} FitdAnimInfo;
int fitd_body_parse(const uint8_t *src,size_t len,FitdBody *out);
bool fitd_body_load(FitdBody *b,const char *directory,unsigned index,FILE *log);
#ifndef FITD_HOST_TEST
void fitd_body_draw(const FitdBody *b,Mtx view,Mtx44 projection,
                    float yaw_deg,float x,float z,
                    const FitdPoseVertex *pose);
#endif
void fitd_body_release(FitdBody *b);
bool fitd_animation_info(FitdAnimInfo *info,const char *directory,
                         unsigned index,FILE *log);
#endif
