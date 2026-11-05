/* FITD Wii Test07: host-safe one-pass hardware/asset diagnostic runner.
 * All game data is read from the user's SD; never bundled.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef FITD_WII_SUITE_H
#define FITD_WII_SUITE_H
#include <stdio.h>
#include <stdbool.h>
#include "fitd_anim.h"
#include "fitd_stage.h"
#include "fitd_scene.h"
#define FITD_SUITE_TOP 8u
typedef struct {
    unsigned clip_count,clips_decoded,clip_poses_ok,clip_pose_fail;
    unsigned cameras_ok,cameras_failed,masks_ok,mask_polygons;
    unsigned movement_steps,movement_blocks,camera_matches;
    unsigned top_count;
    unsigned top_index[FITD_SUITE_TOP];
    float top_score[FITD_SUITE_TOP];
    bool rest_ok;
} FitdSuiteReport;
/* Sorted, score-descending shortlist. Excludes score <= 0. */
void fitd_suite_rank(FitdSuiteReport *s,unsigned index,float score);
/* A single automatic diagnostic pass after loading the real AITD1 PAKs. */
void fitd_suite_run(const char *directory,const FitdBody *body,
    const FitdStage *stage,const FitdStageRoom *room,
    const FitdSceneActor *actor,const unsigned *indices,unsigned count,
    FitdSuiteReport *out,FILE *log);
#endif
