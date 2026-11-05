/*
 * Test04: synthetic LISTANIM 8-byte and 16-byte group formats, no
 * copyrighted assets. Validates time interpolation and group geometry.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "../source/fitd_anim.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
typedef struct {uint8_t b[256];size_t n;} Writer;
static void u16(Writer *w,uint16_t v){
    w->b[w->n++]=(uint8_t)v;
    w->b[w->n++]=(uint8_t)(v>>8);
}
static Writer make_clip(int optimized){
    Writer w={{0},0};
    u16(&w,2);u16(&w,2); /* 2 frames and 2 groups */
    for(unsigned f=0;f<2;f++){
        u16(&w,5);   /* keyframe duration = 5 game ticks */
        u16(&w,(uint16_t)(f*12));u16(&w,0);u16(&w,0);
        /* group 0: rotate Y by 256 (quarter turn) in frame 1 */
        u16(&w,0);
        u16(&w,0);u16(&w,optimized?0:(uint16_t)(f*256));u16(&w,0);
        if(optimized){
            u16(&w,0);u16(&w,(uint16_t)(f*256));u16(&w,0);u16(&w,0);
        }
        /* group 1: translate X by 40 in frame 1 */
        u16(&w,1);
        u16(&w,(uint16_t)(f*40));u16(&w,0);u16(&w,0);
        if(optimized){
            u16(&w,0);u16(&w,0);u16(&w,0);u16(&w,0);
        }
    }
    return w;
}
static int test_case(int optimized){
    const Writer raw=make_clip(optimized);
    FitdAnimClip a={0};
    int status=fitd_anim_parse(raw.b,raw.n,&a);
    if(status!=0 || !a.valid || a.frame_count!=2 ||
       a.group_count!=2 || a.optimized!=(optimized!=0) ||
       a.total_ticks!=10){
        printf("anim decode failed optimized=%d status=%d\n",optimized,status);
        fitd_anim_release(&a);
        return 1;
    }
    FitdBody *b=(FitdBody*)calloc(1,sizeof(FitdBody));
    if(!b){fitd_anim_release(&a);return 1;}
    b->valid=true;b->flags=optimized?10:2;
    b->vertex_count=3;b->group_count=2;
    b->group_order[0]=0;b->group_order[1]=1;
    b->vertices[0].x=0;b->vertices[1].x=100;b->vertices[2].x=20;
    b->groups[0].first=0;b->groups[0].count=2;b->groups[0].base=2;
    b->groups[1].first=2;b->groups[1].count=1;b->groups[1].base=0;
    FitdPoseVertex v0[FITD_BODY_MAX_VERTICES];
    FitdPoseVertex v1[FITD_BODY_MAX_VERTICES];
    FitdAnimSample sample={0};
    int ok=fitd_anim_can_apply(&a,b) &&
           fitd_anim_pose(&a,b,1.0,v0,FITD_BODY_MAX_VERTICES,&sample) &&
           sample.animated && sample.to_frame==0 &&
           fabsf(sample.fraction-0.2f)<.001f &&
           fitd_anim_pose(&a,b,7.5,v1,FITD_BODY_MAX_VERTICES,&sample) &&
           sample.animated && sample.to_frame==1 &&
           fabsf(sample.fraction-0.5f)<.001f &&
           isfinite(v1[1].x) && isfinite(v1[1].z) &&
           fabsf(v1[1].x-v0[1].x)>0.1f;
    if(ok){
        b->flags=0;
        ok=!fitd_anim_can_apply(&a,b) &&
           !fitd_anim_pose(&a,b,0,v0,FITD_BODY_MAX_VERTICES,&sample);
    }
    if(!ok)printf("animation sampling/regression FAILED optimized=%d\n",optimized);
    free(b);
    fitd_anim_release(&a);
    return ok?0:1;
}
/*
 * Regression for the deformation reported on physical Wii in Test06:
 * physical BODY groups 0/1 do not have to match GROUP ORDER [1,0].
 * The animation states are indexed by actual group number, while the
 * order table only dictates which transform is applied first.
 */
static int test_physical_group_mapping(void){
    FitdBody *b=(FitdBody*)calloc(1,sizeof(*b));
    if(!b)return 1;
    b->valid=true;b->flags=2;b->vertex_count=3;b->group_count=2;
    b->group_order[0]=1;b->group_order[1]=0;
    b->vertices[0].z=10;
    b->vertices[1].x=100;
    b->vertices[2].x=0;
    b->groups[0].first=0;b->groups[0].count=1;b->groups[0].base=2;
    b->groups[0].parent=-1;b->groups[0].group=1;
    b->groups[1].first=1;b->groups[1].count=1;b->groups[1].base=2;
    b->groups[1].parent=-1;b->groups[1].group=2;
    FitdAnimFrame f={.duration=2};
    FitdAnimGroupState states[2]={0};
    states[0].type=0;states[0].delta[1]=256;
    states[1].type=1;states[1].delta[0]=20;
    FitdAnimClip a={0};
    a.valid=true;a.group_count=2;a.frame_count=1;
    a.total_ticks=2;a.frames=&f;a.states=states;
    FitdPoseVertex out[3]={0};
    FitdAnimSample sample={0};
    int ok=fitd_anim_pose(&a,b,0,out,3,&sample) &&
        fabsf(out[0].x+10.f)<0.01f &&
        fabsf(out[0].z)<0.01f &&
        fabsf(out[1].x-120.f)<0.01f;
    if(!ok)printf("physical group index vs order table FAILED\n");
    free(b);
    return ok?0:1;
}
/*
 * Hierarchy regression: root rotation must rotate a child vertex BEFORE
 * the child's own local state and base vertex have been assembled.
 * Actor mode must not double-apply BODY group-0 root yaw.
 */
static int test_child_rotation_and_actor_yaw(void){
    FitdBody *b=(FitdBody*)calloc(1,sizeof(*b));
    if(!b)return 1;
    b->valid=true;b->flags=2;b->vertex_count=3;b->group_count=2;
    b->group_order[0]=0;b->group_order[1]=1;
    b->vertices[0].x=100;
    b->vertices[1].z=10;
    b->groups[0].first=0;b->groups[0].count=1;b->groups[0].base=2;
    b->groups[0].parent=-1;b->groups[0].group=1;
    b->groups[1].first=1;b->groups[1].count=1;b->groups[1].base=2;
    b->groups[1].parent=1;b->groups[1].group=2;
    FitdAnimFrame f={.duration=2};
    FitdAnimGroupState states[2]={0};
    states[0].type=0;states[0].delta[1]=256;
    states[1].type=1;
    FitdAnimClip a={0};
    a.valid=true;a.group_count=2;a.frame_count=1;
    a.total_ticks=2;a.frames=&f;a.states=states;
    FitdPoseVertex out[3]={0};
    FitdAnimSample sample={0};
    int ok=fitd_anim_pose(&a,b,0,out,3,&sample) &&
        fabsf(out[1].x+10.f)<0.01f && fabsf(out[1].z)<0.01f;
    ok=ok && fitd_anim_pose_actor(&a,b,0,out,3,&sample) &&
        fabsf(out[1].x)<0.01f && fabsf(out[1].z-10.f)<0.01f;
    if(!ok)printf("child subtree rotation or actor root yaw FAILED\n");
    free(b);
    return ok?0:1;
}
static int test_rotation_axis_order(void){
    FitdBody *b=(FitdBody*)calloc(1,sizeof(*b));
    if(!b)return 1;
    b->valid=true;b->flags=2;b->vertex_count=2;b->group_count=1;
    b->vertices[0].x=10;
    b->groups[0].first=0;b->groups[0].count=1;b->groups[0].base=1;
    b->groups[0].parent=-1;b->groups[0].group=1;
    FitdAnimFrame f={.duration=2};
    FitdAnimGroupState state={0};
    state.type=0;state.delta[0]=256;state.delta[1]=256;state.delta[2]=256;
    FitdAnimClip a={0};
    a.valid=true;a.group_count=1;a.frame_count=1;
    a.total_ticks=2;a.frames=&f;a.states=&state;
    FitdPoseVertex out[2]={0};
    int ok=fitd_anim_pose(&a,b,0,out,2,NULL) &&
           fabsf(out[0].x-10.f)<0.02f &&
           fabsf(out[0].y)<0.02f && fabsf(out[0].z)<0.02f;
    if(!ok)printf("original Y/X/Z Euler order FAILED (twisted geometry)\n");
    free(b);
    return ok?0:1;
}
int main(void){
    int failures=0;
    failures+=test_case(0);
    failures+=test_case(1);
    failures+=test_physical_group_mapping();
    failures+=test_child_rotation_and_actor_yaw();
    failures+=test_rotation_axis_order();
    const Writer good=make_clip(0);
    FitdAnimClip empty={0};
    if(fitd_anim_parse(good.b,good.n-1,&empty)>=0)failures++;
    if(fitd_anim_parse(good.b,3,&empty)>=0)failures++;
    uint8_t invalid[256];
    memcpy(invalid,good.b,good.n);
    invalid[0]=0;invalid[1]=0; /* zero frames */
    if(fitd_anim_parse(invalid,good.n,&empty)>=0)failures++;
    memcpy(invalid,good.b,good.n);
    invalid[12]=0xFF;invalid[13]=0xFF; /* invalid keyframe type */
    if(fitd_anim_parse(invalid,good.n,&empty)>=0)failures++;
    fitd_anim_release(&empty);
    if(failures){
        printf("FITD Wii Test04 animation regressions FAILED: %d\n",failures);
        return 1;
    }
    puts("FITD Wii Test04 animation parser/interpolation: PASS");
    return 0;
}
