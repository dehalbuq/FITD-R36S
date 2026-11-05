/* Synthetic idle/walk animator transitions: no original game assets. */
#include "../source/fitd_motion.h"
#include <math.h>
#include <stdio.h>
int main(void){
    FitdMotionController c={0};
    fitd_motion_reset(&c);
    if(c.kind!=FITD_MOTION_IDLE || c.ticks!=0 || c.transitions)return 1;
    fitd_motion_step(&c,false,20,true);
    if(fabs(c.ticks-1.0)>0.001 || c.transitions!=0)return 2;
    fitd_motion_step(&c,true,20,true);
    if(c.kind!=FITD_MOTION_WALK || c.ticks!=0.0 ||
       c.transitions!=1 || c.walking_updates!=1)return 3;
    fitd_motion_step(&c,true,100,true);
    if(fabs(c.ticks-5.0)>0.001)return 4;
    fitd_motion_step(&c,true,500,true);
    if(fabs(c.ticks-5.0)>0.001)return 5; /* reject huge timer gaps */
    fitd_motion_step(&c,true,40,false);
    if(fabs(c.ticks-5.0)>0.001)return 6; /* pause animation */
    fitd_motion_step(&c,false,20,true);
    if(c.kind!=FITD_MOTION_IDLE || c.ticks!=0.0 ||
       c.transitions!=2)return 7;
    fitd_motion_step(&c,false,80,true);
    if(fabs(c.ticks-4.0)>0.001)return 8;
    puts("FITD Wii Test06.2 idle/walk state changes, no damage loop, 50 Hz timing: PASS");
    return 0;
}
