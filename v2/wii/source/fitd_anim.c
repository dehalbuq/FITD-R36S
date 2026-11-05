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
/*
 * The original renderer rotates Y, then X, then Z. The old Wii preview
 * used Y/Z/X, which bent joints with two or three simultaneous rotations.
 * FITD uses 1024 angle units per revolution; no bone-length scaling.
 */
static void rotate_angles(float *x,float *y,float *z,
                          float ax,float ay,float az){
    const float sx=sinf(ax*TAU_OVER_1024),cx=cosf(ax*TAU_OVER_1024);
    const float sy=sinf(ay*TAU_OVER_1024),cy=cosf(ay*TAU_OVER_1024);
    const float sz=sinf(az*TAU_OVER_1024),cz=cosf(az*TAU_OVER_1024);
    float X=*x,Y=*y,Z=*z;
    float t=X;
    X=cy*X-sy*Z;Z=sy*t+cy*Z; /* yaw */
    t=Y;
    Y=cx*Y-sx*Z;Z=sx*t+cx*Z; /* pitch */
    t=X;
    X=cz*X-sz*Y;Y=sz*t+cz*Y; /* roll */
    *x=X;*y=Y;*z=Z;
}
static bool group_range_valid(const FitdBody *body,unsigned idx){
    if(idx>=body->group_count)return false;
    const FitdGroup *group=&body->groups[idx];
    return group->first>=0 && group->count>=0 &&
           (unsigned)group->first<=body->vertex_count &&
           (unsigned)group->count<=body->vertex_count-(unsigned)group->first;
}
static void rotate_group_points(const FitdGroup *g,FitdPoseVertex *v,
                                const float r[3]){
    if(r[0]==0.f && r[1]==0.f && r[2]==0.f)return;
    for(unsigned j=0;j<(unsigned)g->count;j++){
        FitdPoseVertex *p=&v[(unsigned)g->first+j];
        rotate_angles(&p->x,&p->y,&p->z,r[0],r[1],r[2]);
    }
}
/*
 * AITD1 RotateGroupe() rotates the chosen group AND descendants whose
 * m_orgGroup points to the parent's m_numGroup. The original searches
 * only later group records. Descendants must rotate before their own
 * keyframe state is applied, while vertices are still group-local.
 *
 * Iterate forward only: malformed parent IDs cannot introduce cycles.
 * A group is visited at most once per ancestor, and depth is bounded.
 */
static void rotate_group_tree(const FitdBody *b,unsigned index,
                              FitdPoseVertex *v,const float r[3],
                              unsigned depth){
    if(depth>=b->group_count || !group_range_valid(b,index))return;
    const FitdGroup *parent=&b->groups[index];
    rotate_group_points(parent,v,r);
    for(unsigned child=index+1;child<b->group_count;child++){
        const FitdGroup *next=&b->groups[child];
        if(next->parent==parent->group && next->group!=parent->group &&
           group_range_valid(b,child))
            rotate_group_tree(b,child,v,r,depth+1u);
    }
}
static bool pose_impl(const FitdAnimClip *a,const FitdBody *b,
                      double ticks,FitdPoseVertex *v,size_t cap,
                      FitdAnimSample *sample,bool actor_root){
    if(sample)memset(sample,0,sizeof(*sample));
    if(!b || !v || !fitd_anim_can_apply(a,b) ||
       cap<b->vertex_count || !a->total_ticks)return false;
    for(unsigned i=0;i<b->vertex_count;i++){
        v[i].x=(float)b->vertices[i].x;
        v[i].y=(float)b->vertices[i].y;
        v[i].z=(float)b->vertices[i].z;
    }
    if(!isfinite(ticks) || ticks<0.0)ticks=0.0;
    double phase=fmod(ticks,(double)a->total_ticks);
    if(phase<0.0)phase=0.0;
    unsigned to=0;
    double beginning=0.0;
    for(;to+1<a->frame_count;to++){
        const unsigned duration=a->frames[to].duration?
                                a->frames[to].duration:1u;
        if(phase<beginning+duration)break;
        beginning+=duration;
    }
    const unsigned from=(to==0u)?((unsigned)a->frame_count-1u):(to-1u);
    const unsigned duration=a->frames[to].duration?
                            a->frames[to].duration:1u;
    float t=(float)((phase-beginning)/(double)duration);
    if(t<0.f)t=0.f;
    if(t>1.f)t=1.f;
    if(sample){
        sample->from_frame=(uint16_t)from;
        sample->to_frame=(uint16_t)to;
        sample->fraction=t;
        sample->compatible=true;
        sample->animated=true;
        for(unsigned k=0;k<3;k++)
            sample->step[k]=(int16_t)linear(
                a->frames[from].step[k],a->frames[to].step[k],t);
    }

    /*
     * KEY FIX: SetAnimObjet/SetInterAnimObjet write animation state
     * directly into m_groups[i]. The order table ONLY determines the
     * order those group states are APPLIED to vertices by AnimNuage.
     *
     * Previous Wii code indexed a->states[g] but transformed
     * b->groups[b->group_order[g]], mismatching limbs and rotations.
     */
    for(unsigned slot=0;slot<b->group_count;slot++){
        const unsigned index=b->group_order[slot];
        if(!group_range_valid(b,index))return false;
        const FitdGroup *group=&b->groups[index];
        const bool has_anim=index<a->group_count;
        const FitdAnimGroupState *before=has_anim?
            &a->states[(size_t)from*a->group_count+index]:NULL;
        const FitdAnimGroupState *after=has_anim?
            &a->states[(size_t)to*a->group_count+index]:NULL;
        const int type=has_anim?after->type:group->kind;
        if(type<0 || type>2)return false;
        float delta[3]={0,0,0},rot[3]={0,0,0};
        const int16_t defaults[3]={group->dx,group->dy,group->dz};
        const int16_t default_rot[3]={group->rx,group->ry,group->rz};
        for(unsigned axis=0;axis<3;axis++){
            const float old=has_anim?(float)before->delta[axis]:
                                       (float)defaults[axis];
            const float target=has_anim?(float)after->delta[axis]:
                                          (float)defaults[axis];
            delta[axis]=(type==0)?
                interpolate_angle(old,target,t):linear(old,target,t);
            if(a->optimized){
                const float rold=has_anim?(float)before->rotate[axis]:
                                            (float)default_rot[axis];
                const float rtarget=has_anim?(float)after->rotate[axis]:
                                               (float)default_rot[axis];
                rot[axis]=interpolate_angle(rold,rtarget,t);
            }else if(type==0){
                rot[axis]=delta[axis];
            }
        }
        if(actor_root && index==0u && type==0){
            /* Original AnimNuage overwrites non-optimized BODY group zero
               with actor alpha/beta/gamma. Actor yaw is already applied
               by fitd_scene_project, so don't rotate it again here. */
            rot[0]=0.f;rot[1]=0.f;rot[2]=0.f;
        }
        switch(type){
        case 0:
            if(a->optimized)
                rotate_group_points(group,v,rot);
            else
                rotate_group_tree(b,index,v,rot,0u);
            break;
        case 1:
            for(unsigned j=0;j<(unsigned)group->count;j++){
                FitdPoseVertex *p=&v[(unsigned)group->first+j];
                p->x+=delta[0];p->y+=delta[1];p->z+=delta[2];
            }
            if(a->optimized)rotate_group_points(group,v,rot);
            break;
        case 2:
            for(unsigned j=0;j<(unsigned)group->count;j++){
                FitdPoseVertex *p=&v[(unsigned)group->first+j];
                p->x*=(delta[0]+256.f)/256.f;
                p->y*=(delta[1]+256.f)/256.f;
                p->z*=(delta[2]+256.f)/256.f;
            }
            if(a->optimized)rotate_group_points(group,v,rot);
            break;
        }
    }
    /*
     * FITD's AnimNuage applies base vertex offsets in physical BODY
     * group order AFTER applying each local skeletal transformation.
     * This is essential: a limb's parent position has been transformed
     * by its ancestors before the final world-relative assembly.
     */
    for(unsigned i=0;i<b->group_count;i++){
        const FitdGroup *g=&b->groups[i];
        if(!group_range_valid(b,i) || g->base<0 ||
           (unsigned)g->base>=b->vertex_count)return false;
        const FitdPoseVertex base=v[g->base];
        for(unsigned j=0;j<(unsigned)g->count;j++){
            FitdPoseVertex *p=&v[(unsigned)g->first+j];
            p->x+=base.x;p->y+=base.y;p->z+=base.z;
        }
    }
    for(unsigned i=0;i<b->vertex_count;i++){
        if(!isfinite(v[i].x)||!isfinite(v[i].y)||!isfinite(v[i].z))
            return false;
    }
    return true;
}
bool fitd_anim_pose(const FitdAnimClip *a,const FitdBody *b,
                    double ticks,FitdPoseVertex *v,size_t cap,
                    FitdAnimSample *sample){
    return pose_impl(a,b,ticks,v,cap,sample,false);
}
bool fitd_anim_pose_actor(const FitdAnimClip *a,const FitdBody *b,
                          double ticks,FitdPoseVertex *v,size_t cap,
                          FitdAnimSample *sample){
    return pose_impl(a,b,ticks,v,cap,sample,true);
}


/*
 * Reconstruct the BODY's bind/rest state through the same physical group
 * ordering, parent rotations and final base-vertex assembly as animation.
 * Raw BODY vertices are GROUP-LOCAL and must not be rendered directly as
 * the actor's idle pose; doing so collapsed Edward in Wii Test06.2.
 * Zero game asset data is supplied by this program.
 */
bool fitd_anim_pose_rest_actor(const FitdBody *b,FitdPoseVertex *v,
                               size_t cap,FitdAnimSample *sample){
    if(sample)memset(sample,0,sizeof(*sample));
    if(!b || !b->valid || !v || cap<b->vertex_count ||
       b->group_count==0 || b->group_count>FITD_BODY_MAX_GROUPS)
        return false;
    FitdAnimFrame frame={0};
    frame.duration=1;
    FitdAnimGroupState groups[FITD_BODY_MAX_GROUPS]={0};
    for(unsigned i=0;i<b->group_count;i++){
        const FitdGroup *g=&b->groups[i];
        if(g->kind<0 || g->kind>2)return false;
        groups[i].type=g->kind;
        groups[i].delta[0]=g->dx;
        groups[i].delta[1]=g->dy;
        groups[i].delta[2]=g->dz;
        groups[i].rotate[0]=g->rx;
        groups[i].rotate[1]=g->ry;
        groups[i].rotate[2]=g->rz;
    }
    FitdAnimClip rest={0};
    rest.valid=true;
    rest.frame_count=1;
    rest.group_count=b->group_count;
    rest.total_ticks=1;
    rest.optimized=(b->flags & OPTIMIZED_FLAG)!=0;
    rest.frames=&frame;
    rest.states=groups;
    const bool result=fitd_anim_pose_actor(&rest,b,0.0,v,cap,sample);
    if(sample){
        sample->animated=false;
        sample->compatible=result;
    }
    return result;
}

/* Read the user's original archive once and list structurally valid
 * animations for this exact BODY (20 groups for Edward's BODY 15).
 * This does not assign semantic labels like "walking" without visual proof.
 * No archive data is modified or packed into the Wii release. */
unsigned fitd_anim_scan_body(const char *directory,const FitdBody *b,
                              unsigned *indices,unsigned capacity,FILE *log){
    if(!directory || !b || !b->valid || !indices || capacity==0 ||
       b->group_count==0)return 0;
    char path[256];
    snprintf(path,sizeof(path),"%s/LISTANIM.PAK",directory);
    FILE *f=fopen(path,"rb");
    if(!f){
        if(log)fprintf(log,"FITD-WII-SCAN06-3 error=open\n");
        return 0;
    }
    FitdPakDirectory dir={0};
    const int directory_status=fitd_pak_directory(f,&dir);
    if(directory_status){
        if(log)fprintf(log,"FITD-WII-SCAN06-3 error=directory status=%d\n",directory_status);
        fclose(f);
        return 0;
    }
    const bool target_optimized=(b->flags & OPTIMIZED_FLAG)!=0;
    unsigned valid=0,stored=0,invalid=0;
    for(unsigned index=0;index<dir.count;index++){
        FitdPakEntry entry={0};
        if(fitd_pak_entry(f,&dir,index,&entry)!=0){invalid++;continue;}
        uint8_t *raw=NULL;
        size_t bytes=0;
        if(fitd_pak_load(f,&entry,&raw,&bytes)!=0){
            invalid++;
            free(raw);
            continue;
        }
        if(bytes>=4u){
            const unsigned frames=fitd_read_le16(raw);
            const unsigned groups=fitd_read_le16(raw+2);
            const size_t expected=4u+(size_t)frames*
                (8u+(size_t)groups*(target_optimized?16u:8u));
            if(frames>0 && frames<=FITD_ANIM_MAX_FRAMES &&
               groups==b->group_count && bytes==expected){
                FitdAnimClip candidate={0};
                if(fitd_anim_parse(raw,bytes,&candidate)==0 &&
                   fitd_anim_can_apply(&candidate,b)){
                    valid++;
                    if(stored<capacity)indices[stored++]=index;
                    if(log)fprintf(log,
                        "FITD-WII-SCAN06-3 candidate=%u groups=%u frames=%u ticks=%lu optimized=%d\n",
                        index,groups,frames,(unsigned long)candidate.total_ticks,
                        candidate.optimized?1:0);
                }else invalid++;
                fitd_anim_release(&candidate);
            }
        }
        free(raw);
    }
    fclose(f);
    if(log){
        fprintf(log,
            "FITD-WII-SCAN06-3 summary entries=%lu valid=%u stored=%u invalid=%u body=%u groups=%u truncated=%d\n",
            (unsigned long)dir.count,valid,stored,invalid,b->index,
            b->group_count,valid>stored?1:0);
        fflush(log);
    }
    return stored;
}
