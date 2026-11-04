/*
 * Native Wii / host-safe LISTANIM.PAK keyframe decoder and interpolation.
 * The original FITD stores per-frame DURATIONS (not absolute timestamps).
 * Adapted from yaz0r/FITD: FitdLib/hqr.cpp/createAnimationFromPtr() and
 * anim.cpp/SetInterAnimObjet(). No game assets included.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "fitd_anim.h"
#include "fitd_pak.h"
#include "fitd_endian.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define ANIMATED_FLAG 2u
#define OPTIMIZED_FLAG 8u
#define TAU_OVER_1024 0.006135923151543f

void fitd_anim_release(FitdAnimClip *c){
    if(!c)return;
    free(c->frames);
    free(c->states);
    memset(c,0,sizeof(*c));
}
int fitd_anim_parse(const uint8_t *raw,size_t len,FitdAnimClip *out){
    if(!out || !raw || len<12)return -1;
    const unsigned frames=fitd_read_le16(raw);
    const unsigned groups=fitd_read_le16(raw+2);
    if(frames==0 || frames>FITD_ANIM_MAX_FRAMES ||
       groups==0 || groups>FITD_BODY_MAX_GROUPS)return -2;
    const size_t row8=8u+groups*8u;
    const size_t row16=8u+groups*16u;
    const int optimized=(len==4u+(size_t)frames*row16);
    if(!optimized && len!=4u+(size_t)frames*row8)return -3;
    FitdAnimClip tmp={0};
    tmp.frames=(FitdAnimFrame*)calloc(frames,sizeof(*tmp.frames));
    tmp.states=(FitdAnimGroupState*)calloc((size_t)frames*groups,
                                          sizeof(*tmp.states));
    if(!tmp.frames || !tmp.states){
        fitd_anim_release(&tmp);
        return -4;
    }
    tmp.frame_count=(uint16_t)frames;
    tmp.group_count=(uint16_t)groups;
    tmp.optimized=optimized!=0;
    size_t pos=4;
    for(unsigned f=0;f<frames;f++){
        FitdAnimFrame *fr=&tmp.frames[f];
        fr->duration=fitd_read_le16(raw+pos);pos+=2;
        for(unsigned k=0;k<3;k++){
            fr->step[k]=(int16_t)fitd_read_le16(raw+pos);
            pos+=2;
        }
        /* Treat a zero-duration frame as a 1-tick hold in the viewer;
           this avoids division by zero without altering source data. */
        tmp.total_ticks+=(fr->duration?fr->duration:1u);
        for(unsigned g=0;g<groups;g++){
            FitdAnimGroupState *state=&tmp.states[f*groups+g];
            state->type=(int16_t)fitd_read_le16(raw+pos);pos+=2;
            for(unsigned k=0;k<3;k++){
                state->delta[k]=(int16_t)fitd_read_le16(raw+pos);
                pos+=2;
            }
            if(optimized){
                for(unsigned k=0;k<3;k++){
                    state->rotate[k]=(int16_t)fitd_read_le16(raw+pos);
                    pos+=2;
                }
                pos+=2; /* original 2-byte reserved state */
            }
            if(state->type<0 || state->type>2){
                fitd_anim_release(&tmp);
                return -5;
            }
        }
    }
    if(pos!=len){
        fitd_anim_release(&tmp);
        return -6;
    }
    tmp.valid=true;
    fitd_anim_release(out);
    *out=tmp;
    return 0;
}
bool fitd_anim_load(FitdAnimClip *out,const char *directory,
                    unsigned index,FILE *log){
    if(!out || !directory)return false;
    char path[256];
    snprintf(path,sizeof(path),"%s/LISTANIM.PAK",directory);
    FILE *f=fopen(path,"rb");
    if(!f){
        if(log){fprintf(log,"FITD-WII-ANIM04 missing %s\n",path);fflush(log);}
        return false;
    }
    FitdPakDirectory dir={0};
    FitdPakEntry entry={0};
    int status=fitd_pak_directory(f,&dir);
    if(status==0)status=fitd_pak_entry(f,&dir,index,&entry);
    uint8_t *raw=NULL;
    size_t bytes=0;
    if(status==0)status=fitd_pak_load(f,&entry,&raw,&bytes);
    fclose(f);
    if(status!=0){
        if(log){fprintf(log,
            "FITD-WII-ANIM04 load index=%u status=%s\n",
            index,fitd_pak_error(status));fflush(log);}
        free(raw);
        return false;
    }
    FitdAnimClip next={0};
    int parsed=fitd_anim_parse(raw,bytes,&next);
    free(raw);
    if(parsed!=0){
        if(log){fprintf(log,
            "FITD-WII-ANIM04 parse index=%u status=%d unpacked=%lu\n",
            index,parsed,(unsigned long)bytes);fflush(log);}
        return false;
    }
    next.index=index;
    next.archive_count=dir.count;
    fitd_anim_release(out);
    *out=next;
    if(log){
        fprintf(log,
          "FITD-WII-ANIM04 OK index=%u/%lu frames=%u groups=%u "
          "optimized=%d ticks=%lu\n",
          index,(unsigned long)dir.count,
          (unsigned)out->frame_count,(unsigned)out->group_count,
          out->optimized?1:0,(unsigned long)out->total_ticks);
        fflush(log);
    }
    return true;
}
bool fitd_anim_can_apply(const FitdAnimClip *a,const FitdBody *b){
    return a && b && a->valid && b->valid &&
           (b->flags & ANIMATED_FLAG)!=0 &&
           a->group_count>0 &&
           a->group_count<=b->group_count &&
           a->optimized==((b->flags & OPTIMIZED_FLAG)!=0);
}
static float linear(float a,float b,float t){
    return a+(b-a)*t;
}
static float interpolate_angle(float a,float b,float t){
    /* FITD angles are 10-bit full turns; take shortest interpolation. */
    float d=b-a;
    while(d>512.f)d-=1024.f;
    while(d< -512.f)d+=1024.f;
    return a+d*t;
}
static void rotate_angles(float *x,float *y,float *z,
                          float ax,float ay,float az){
    const float sx=sinf(ax*TAU_OVER_1024),cx=cosf(ax*TAU_OVER_1024);
    const float sy=sinf(ay*TAU_OVER_1024),cy=cosf(ay*TAU_OVER_1024);
    const float sz=sinf(az*TAU_OVER_1024),cz=cosf(az*TAU_OVER_1024);
    /* Order of rotation Y/Z/X used by legacy FITD geometry. */
    float X=*x,Y=*y,Z=*z;
    float t=X;
    X=cy*X-sy*Z;Z=sy*t+cy*Z;
    t=X;X=cz*X-sz*Y;Y=sz*t+cz*Y;
    t=Y;Y=cx*Y-sx*Z;Z=sx*t+cx*Z;
    *x=X;*y=Y;*z=Z;
}
bool fitd_anim_pose(const FitdAnimClip *a,const FitdBody *b,
                    double ticks,FitdPoseVertex *v,size_t cap,
                    FitdAnimSample *sample){
    if(sample)memset(sample,0,sizeof(*sample));
    if(!b || !v || !fitd_anim_can_apply(a,b) ||
       cap<b->vertex_count || !a->total_ticks)return false;
    for(unsigned i=0;i<b->vertex_count;i++){
        v[i].x=(float)b->vertices[i].x;
        v[i].y=(float)b->vertices[i].y;
        v[i].z=(float)b->vertices[i].z;
    }
    if(ticks<0.0)ticks=0;
    double phase=fmod(ticks,(double)a->total_ticks);
    if(phase<0.0)phase=0.0;
    unsigned to=0;
    double beginning=0;
    for(;to+1<a->frame_count;to++){
        unsigned duration=a->frames[to].duration?
                          a->frames[to].duration:1u;
        if(phase<beginning+duration)break;
        beginning+=duration;
    }
    const unsigned from=(to==0)?a->frame_count-1:to-1;
    const unsigned duration=a->frames[to].duration?
                            a->frames[to].duration:1u;
    float t=(float)((phase-beginning)/duration);
    if(t<0.f)t=0.f;
    if(t>1.f)t=1.f;
    if(sample){
        sample->from_frame=from;
        sample->to_frame=to;
        sample->fraction=t;
        sample->compatible=true;
        sample->animated=true;
        for(unsigned k=0;k<3;k++)
            sample->step[k]=(int16_t)linear(
                a->frames[from].step[k],a->frames[to].step[k],t);
    }

    /*
     * Per-group interpolated rotations/translations from genuine LISTANIM.
     * This is a first-pass skeletal PREVIEW: child hierarchy, actor positioning
     * and movement semantics still need integration with the real engine.
     * Crucially, it does not replay a canned animation or change FPS timing.
     */
    for(unsigned g=0;g<a->group_count;g++){
        if(b->group_order[g]>=b->group_count)return false;
        const FitdGroup *group=&b->groups[b->group_order[g]];
        if(group->first<0 || group->count<0 ||
           (unsigned)group->first+(unsigned)group->count>b->vertex_count)
            return false;
        const FitdAnimGroupState *before=
            &a->states[(size_t)from*a->group_count+g];
        const FitdAnimGroupState *after=
            &a->states[(size_t)to*a->group_count+g];
        const int type=after->type;
        float transform[3];
        for(unsigned k=0;k<3;k++){
            transform[k]=(type==0)?
                interpolate_angle((float)before->delta[k],
                                  (float)after->delta[k],t):
                linear((float)before->delta[k],
                       (float)after->delta[k],t);
        }
        float rot[3]={0,0,0};
        if(a->optimized){
            for(unsigned k=0;k<3;k++)
                rot[k]=interpolate_angle((float)before->rotate[k],
                                         (float)after->rotate[k],t);
        }else if(type==0){
            for(unsigned k=0;k<3;k++)rot[k]=transform[k];
        }
        for(unsigned j=0;j<(unsigned)group->count;j++){
            const unsigned i=(unsigned)group->first+j;
            float x=v[i].x,y=v[i].y,z=v[i].z;
            if(type==1){
                x+=transform[0];y+=transform[1];z+=transform[2];
            }else if(type==2){
                x*=1.f+transform[0]/256.f;
                y*=1.f+transform[1]/256.f;
                z*=1.f+transform[2]/256.f;
            }
            if(type==0 || a->optimized)
                rotate_angles(&x,&y,&z,rot[0],rot[1],rot[2]);
            v[i].x=x;v[i].y=y;v[i].z=z;
        }
    }
    /* Base vertices compose hierarchical BODY positions in the engine.
       Add base offset after local group deltas, as in AnimNuage(). */
    for(unsigned g=0;g<b->group_count;g++){
        const FitdGroup *group=&b->groups[g];
        if(group->base<0 || group->base>=b->vertex_count)return false;
        const float x=v[group->base].x,y=v[group->base].y,z=v[group->base].z;
        for(unsigned j=0;j<(unsigned)group->count;j++){
            FitdPoseVertex *p=&v[(unsigned)group->first+j];
            p->x+=x;p->y+=y;p->z+=z;
        }
    }
    return true;
}
