/*
 * Native AITD1 actor/camera projection, independent of graphics backend.
 * Original camera translation and focal projection follow:
 * yaz0r/FITD FitdLib/main.cpp InitView + renderer.cpp AnimNuage.
 * Prototype for one controllable model, not full actor/script runtime.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef FITD_WII_SCENE_H
#define FITD_WII_SCENE_H
#include <stdbool.h>
#include "fitd_stage.h"
#include "fitd_body.h"
typedef struct {
    float x,y,z;            /* original FITD room-coordinate units */
    unsigned yaw;           /* 1024 units = 360 degrees */
    unsigned collisions;    /* hard collision rejections */
    unsigned steps;
} FitdSceneActor;
typedef struct {
    float x,y,depth;
    bool visible;
} FitdSceneScreenPoint;
bool fitd_scene_project(const FitdStageCamera *camera,
                        const FitdStageRoom *room,
                        const FitdSceneActor *actor,
                        float body_x,float body_y,float body_z,
                        FitdSceneScreenPoint *out);
bool fitd_scene_move(FitdSceneActor *actor,const FitdStageRoom *room,
                     float dx,float dz);
void fitd_scene_bounds(const FitdSceneActor *actor,
                       int *x1,int *x2,int *z1,int *z2);
void fitd_scene_choose_spawn(FitdSceneActor *actor,const FitdStage *stage,
                             const FitdStageRoom *room,
                             unsigned camera,FILE *log);
#endif
