/*
 * Tiny testable gameplay-animation controller for the Wii prototype.
 * In the full FITD runtime, ANIM_MOVE and LIFE 549/550 scripts will
 * determine idle/walk/run/turn/action animations instead.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "fitd_motion.h"
#include <string.h>
void fitd_motion_reset(FitdMotionController *state){
    if(!state)return;
    memset(state,0,sizeof(*state));
}
void fitd_motion_step(FitdMotionController *state,bool moved,
                      uint64_t elapsed_ms,bool playing){
    if(!state)return;
    const FitdMotionKind desired=moved?FITD_MOTION_WALK:FITD_MOTION_IDLE;
    if(state->kind!=desired){
        state->kind=desired;
        state->ticks=0.0;
        state->transitions++;
    }else if(playing && elapsed_ms<=250u){
        state->ticks+=(double)elapsed_ms*0.05;
    }
    if(moved)state->walking_updates++;
}
