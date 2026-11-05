/* Wii Test07: batch clip interpolation, cinematic cameras, original
 * occlusion masks and collision logic without manual scene switching.
 * Rankings measure geometric motion ONLY; they do not identify walks.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "fitd_suite.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
void fitd_suite_rank(FitdSuiteReport *s,unsigned index,float score){
    if(!s || !isfinite(score) || score<=0.f)return;
    unsigned where=0;
    while(where<s->top_count && s->top_score[where]>=score)where++;
    if(where>=FITD_SUITE_TOP)return;
    unsigned end=s->top_count;
    if(end<FITD_SUITE_TOP)s->top_count++;
    else end=FITD_SUITE_TOP-1u;
    for(unsigned k=end;k>where;k--){
        s->top_score[k]=s->top_score[k-1u];
        s->top_index[k]=s->top_index[k-1u];
    }
    s->top_score[where]=score;
    s->top_index[where]=index;
}
void fitd_suite_run(const char *directory,const FitdBody *body,
    const FitdStage *stage,const FitdStageRoom *room,
    const FitdSceneActor *actor,const unsigned *indices,unsigned count,
    FitdSuiteReport *out,FILE *log){
    if(!out)return;
    memset(out,0,sizeof(*out));
    if(!directory || !body || !stage || !room || !actor ||
       !body->valid || !room->valid || !stage->valid || !indices){
        if(log)fprintf(log,"FITD-WII-SUITE07 status=SKIP reason=missing-stage-body\n");
        return;
    }
    FitdPoseVertex *baseline=(FitdPoseVertex*)calloc(body->vertex_count,sizeof(*baseline));
    FitdPoseVertex *tmp=(FitdPoseVertex*)calloc(body->vertex_count,sizeof(*tmp));
    FitdStageMask *masks=(FitdStageMask*)calloc(1,sizeof(*masks));
    if(!baseline || !tmp || !masks){
        if(log)fprintf(log,"FITD-WII-SUITE07 status=FAIL reason=allocation\n");
        free(baseline);free(tmp);free(masks);return;
    }
    out->rest_ok=fitd_anim_pose_rest_actor(body,baseline,body->vertex_count,NULL);
    out->clip_count=count;
    for(unsigned i=0;i<count;i++){
        FitdAnimClip clip={0};
        const unsigned index=indices[i];
        const bool loaded=fitd_anim_load(&clip,directory,index,NULL);
        const bool compatible=loaded && clip.group_count==body->group_count &&
                              fitd_anim_can_apply(&clip,body);
        if(!compatible){out->clip_pose_fail++;fitd_anim_release(&clip);continue;}
        out->clips_decoded++;
        bool good=true;
        double motion=0.0;
        int32_t forward=0,lateral=0;
        for(unsigned frame=0;frame<clip.frame_count;frame++){
            forward+=clip.frames[frame].step[2];
            lateral+=clip.frames[frame].step[0];
        }
        /* Four different phases, tested against the rest pose. */
        for(unsigned f=0;f<4;f++){
            double tick=(double)clip.total_ticks*(double)f/4.0;
            if(!fitd_anim_pose_actor(&clip,body,tick,tmp,body->vertex_count,NULL)){
                good=false;break;
            }
            for(unsigned v=0;v<body->vertex_count;v++){
                const float dx=tmp[v].x-baseline[v].x;
                const float dy=tmp[v].y-baseline[v].y;
                const float dz=tmp[v].z-baseline[v].z;
                if(!isfinite(dx)||!isfinite(dy)||!isfinite(dz) ||
                   fabsf(tmp[v].x)>30000.f ||
                   fabsf(tmp[v].y)>30000.f ||
                   fabsf(tmp[v].z)>30000.f){
                    good=false;break;
                }
                motion+=(double)dx*dx+(double)dy*dy+(double)dz*dz;
            }
            if(!good)break;
        }
        if(good){
            out->clip_poses_ok++;
            /* Rank dynamic multi-frame clips without guessing whether
               they are walking: a damaged/hurt action can score highly. */
            if(clip.frame_count>1u && clip.total_ticks>1u && index!=301u){
                const float average=(float)sqrt(motion/(4.0*(double)body->vertex_count));
                /* Avoid dominate rankings by grotesque wide poses. */
                if(isfinite(average) && average>5.f && average<600.f)
                    fitd_suite_rank(out,index,average);
            }
        }else out->clip_pose_fail++;
        if(log)fprintf(log,
           "FITD-WII-SUITE07 anim=%u frames=%u ticks=%lu pose=%s "
           "delta=%.1f move_x=%ld move_forward=%ld\n",
           index,clip.frame_count,(unsigned long)clip.total_ticks,
           good?"PASS":"FAIL",
           good?(float)sqrt(motion/(4.0*(double)body->vertex_count)):0.f,
           (long)lateral,(long)forward);
        fitd_anim_release(&clip);
    }
    for(unsigned i=0;i<stage->camera_count;i++){
        FitdStageCamera camera={0};
        if(fitd_stage_camera(stage,i,&camera)!=0){
            out->cameras_failed++;
            if(log)fprintf(log,"FITD-WII-SUITE07 camera=%u status=FAIL\n",i);
            continue;
        }
        out->cameras_ok++;
        int status=fitd_stage_masks_selected(stage,i,(int)room->index,-1,masks,NULL);
        if(status==-4)status=fitd_stage_masks_selected(stage,i,-1,-1,masks,NULL);
        if(status==0){
            out->masks_ok++;
            out->mask_polygons+=masks->polygons;
        }
        FitdSceneScreenPoint point={0};
        const bool projected=fitd_scene_project(&camera,room,actor,0,400,0,&point);
        if(log)fprintf(log,
           "FITD-WII-SUITE07 camera=%u decode=PASS mask=%s polygons=%u pixels=%u origin=%d\n",
           i,status==0?"PASS":"N/A",status==0?masks->polygons:0u,
           status==0?masks->covered_pixels:0u,projected?1:0);
    }
    /* Simulated movement uses COPY of actor, never changes the live game. */
    const float directions[8][2]={{35,0},{-35,0},{0,35},{0,-35},
                                   {35,35},{-35,35},{35,-35},{-35,-35}};
    for(unsigned d=0;d<8;d++){
        FitdSceneActor sim=*actor;
        for(unsigned f=0;f<18;f++){
            if(fitd_scene_move(&sim,room,directions[d][0],directions[d][1]))
                out->movement_steps++;
            else out->movement_blocks++;
            int cam=fitd_stage_camera_auto(stage,room,(int)sim.x,(int)sim.z,0);
            if(cam>=0 && (unsigned)cam<stage->camera_count)
                out->camera_matches++;
        }
    }
    if(log){
        for(unsigned j=0;j<out->top_count;j++)
            fprintf(log,"FITD-WII-SUITE07 shortlist=%u rank=%u score=%.1f semantic=UNKNOWN\n",
                out->top_index[j],j+1u,out->top_score[j]);
        fprintf(log,
           "FITD-WII-SUITE07 SUMMARY rest=%s clips=%u decoded=%u poses_ok=%u "
           "poses_failed=%u camera_ok=%u camera_failed=%u masks_ok=%u "
           "mask_polys=%u simulated_steps=%u blocked=%u camera_resolved=%u "
           "shortlist=%u completed=1\n",
           out->rest_ok?"PASS":"FAIL",out->clip_count,
           out->clips_decoded,out->clip_poses_ok,out->clip_pose_fail,
           out->cameras_ok,out->cameras_failed,out->masks_ok,
           out->mask_polygons,out->movement_steps,out->movement_blocks,
           out->camera_matches,out->top_count);
        fflush(log);
    }
    free(baseline);free(tmp);free(masks);
}
