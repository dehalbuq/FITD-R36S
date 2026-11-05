/*
 * GX renderer projecting authentic original BODY geometry into 320x200
 * fixed CAMERAxx.PAK backgrounds with FITD stage camera coordinates.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef FITD_WII_SCENE_GX_H
#define FITD_WII_SCENE_GX_H
#include <stdbool.h>
#include "fitd_scene.h"
#include "fitd_body.h"
void fitd_scene_gx_draw(const FitdBody *body,const FitdPoseVertex *pose,
                        const FitdStageRoom *room,
                        const FitdStageCamera *camera,
                        const FitdSceneActor *actor,
                        unsigned width,unsigned height,
                        unsigned *projected_vertices);
#endif
