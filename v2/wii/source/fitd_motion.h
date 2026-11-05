/*
 * Wii AITD1 prototype idle/walk motion state.
 * Animation selection is event-driven, so hurt/action clips no longer
 * loop as ordinary locomotion; 50Hz animation timing is frame-independent.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef FITD_WII_MOTION_H
#define FITD_WII_MOTION_H
#include <stdbool.h>
#include <stdint.h>
typedef enum {FITD_MOTION_IDLE=0,FITD_MOTION_WALK=1} FitdMotionKind;
typedef struct {
    FitdMotionKind kind;
    double ticks;
    unsigned transitions;
    unsigned walking_updates;
} FitdMotionController;
void fitd_motion_reset(FitdMotionController *state);
void fitd_motion_step(FitdMotionController *state,bool moved,
                      uint64_t elapsed_ms,bool playing);
#endif
