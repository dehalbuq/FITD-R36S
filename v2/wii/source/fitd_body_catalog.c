/* Only original data installed by the user is read from SD. */
#include "fitd_body_catalog.h"
#include "fitd_anim.h"
#include <stdlib.h>
#include <string.h>
unsigned fitd_body_catalog_scan(const char *directory,
    FitdBodyCandidate *out,unsigned capacity,FILE *log){
    if(!directory || !out || !capacity)return 0;
    FitdBody *body=(FitdBody*)malloc(sizeof(*body));
    FitdPoseVertex *pose=(FitdPoseVertex*)malloc(sizeof(*pose)*FITD_BODY_MAX_VERTICES);
    if(!body || !pose){
        free(body);free(pose);
        if(log)fprintf(log,"FITD-WII-BODYCAT07-1 status=OOM\n");
        return 0;
    }
    unsigned stored=0,found=0,scanned=0;
    /* First 64 BODIES include early protagonist variants. Other actors
       later in the archive are intentionally not auto-selected. */
    for(unsigned i=0;i<64u;i++){
        if(!fitd_body_load(body,directory,i,NULL)){
            fitd_body_release(body);
            continue;
        }
        scanned++;
        if(!fitd_body_catalog_match(body)){fitd_body_release(body);continue;}
        if(!fitd_anim_pose_rest_actor(body,pose,FITD_BODY_MAX_VERTICES,NULL)){
            fitd_body_release(body);continue;
        }
        found++;
        if(stored<capacity){
            out[stored].index=i;
            out[stored].vertices=body->vertex_count;
            out[stored].groups=body->group_count;
            out[stored].primitives=body->primitive_count;
            stored++;
        }
        if(log)fprintf(log,
            "FITD-WII-BODYCAT07-1 candidate=%u groups=%u verts=%u prim=%u "
            "rest=PASS 287_288_candidate=%u\n",
            i,body->group_count,body->vertex_count,body->primitive_count,
            body->group_count==18u?1u:0u);
        fitd_body_release(body);
    }
    if(log){
        fprintf(log,
          "FITD-WII-BODYCAT07-1 SUMMARY scanned=%u matched=%u stored=%u "
          "capacity=%u completed=1\n",scanned,found,stored,capacity);
        fflush(log);
    }
    free(body);free(pose);
    return stored;
}
