/*
 * Original Alone in the Dark LISTANIM.PAK decoder and pose sampler.
 * Wii PowerPC values are BE; original game bytes are LE.
 * Based on FITD by yaz0r, FitdLib/hqr.cpp + FitdLib/anim.cpp.
 * This is an animation PREVIEW; not yet the full FITD game runtime.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef FITD_WII_ANIM_H
#define FITD_WII_ANIM_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "fitd_body.h"

#define FITD_ANIM_MAX_FRAMES 1024
typedef struct {
    int16_t type;
    int16_t delta[3];
    int16_t rotate[3]; /* only for optimized (AITD2+) format */
} FitdAnimGroupState;
typedef struct {
    uint16_t duration; /* original engine: keyframe LENGTH in ticks */
    int16_t step[3];
} FitdAnimFrame;
typedef struct {
    uint16_t frame_count,group_count;
    unsigned index,archive_count;
    bool optimized,valid;
    uint32_t total_ticks;
    FitdAnimFrame *frames;
    FitdAnimGroupState *states; /* frame_count * group_count */
} FitdAnimClip;
typedef struct {
    uint16_t from_frame,to_frame;
    float fraction;
    bool compatible,animated;
    int16_t step[3];
} FitdAnimSample;
int fitd_anim_parse(const uint8_t *raw,size_t len,FitdAnimClip *out);
bool fitd_anim_load(FitdAnimClip *out,const char *directory,unsigned index,FILE *log);
void fitd_anim_release(FitdAnimClip *clip);
bool fitd_anim_can_apply(const FitdAnimClip *anim,const FitdBody *body);
bool fitd_anim_pose(const FitdAnimClip *anim,const FitdBody *body,
                    double ticks,FitdPoseVertex *vertices,size_t capacity,
                    FitdAnimSample *sample);
#endif
