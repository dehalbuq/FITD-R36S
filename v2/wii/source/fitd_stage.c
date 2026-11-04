/*
 * AITD1 native stage decoder, Wii PowerPC big-endian safe.
 * Stage structures from Free in the Dark: floor.cpp / room.cpp;
 * original occlusion polygons from main.cpp/createAITD1Mask.
 * All reads are checked and all data remains on SD untouched.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "fitd_stage.h"
#include "fitd_pak.h"
#include "fitd_endian.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

static bool fits(size_t len,size_t off,size_t n){
    return off<=len && n<=len-off;
}
static uint16_t u16(const uint8_t *data,size_t off){
    return fitd_read_le16(data+off);
}
static int16_t s16(const uint8_t *data,size_t off){
    return (int16_t)u16(data,off);
}
static uint32_t u32(const uint8_t *data,size_t off){
    return fitd_read_le32(data+off);
}
int fitd_stage_blob_count(const uint8_t *data,size_t len,unsigned *count){
    if(!data || !count || len<8)return -1;
    const uint32_t first=u32(data,0);
    if(first<4 || (first&3u)!=0 || first>len ||
       first/4u>FITD_STAGE_MAX_ROOMS)return -2;
    const unsigned n=first/4u;
    uint32_t previous=first;
    for(unsigned i=0;i<n;i++){
        const uint32_t off=u32(data,i*4u);
        if(off<previous || off>len)return -3;
        previous=off;
    }
    *count=n;
    return 0;
}
static int blob_span(const uint8_t *data,size_t size,
                     unsigned count,unsigned index,
                     size_t *start,size_t *end){
    if(!data || !start || !end || index>=count)return -1;
    const size_t a=u32(data,index*4u);
    const size_t b=(index+1<count)?
        (size_t)u32(data,(index+1u)*4u):size;
    if(a>=b || b>size || a<count*4u)return -2;
    *start=a;*end=b;
    return 0;
}
void fitd_stage_release(FitdStage *stage){
    if(!stage)return;
    free(stage->rooms);free(stage->cameras);
    memset(stage,0,sizeof(*stage));
}
/* Takes ownership of both buffers on success only. */
int fitd_stage_set_blobs(FitdStage *stage,
                         uint8_t *room_data,size_t room_bytes,
                         uint8_t *camera_data,size_t camera_bytes){
    if(!stage || !room_data || !camera_data)return -1;
    unsigned rn=0,cn=0;
    if(fitd_stage_blob_count(room_data,room_bytes,&rn)!=0 ||
       fitd_stage_blob_count(camera_data,camera_bytes,&cn)!=0 ||
       rn==0 || cn==0 || cn>FITD_STAGE_MAX_CAMERAS)return -2;
    FitdStage next={0};
    next.rooms=room_data;
    next.rooms_size=room_bytes;
    next.cameras=camera_data;
    next.cameras_size=camera_bytes;
    next.room_count=rn;
    next.camera_count=cn;
    next.valid=true;
    fitd_stage_release(stage);
    *stage=next;
    return 0;
}
bool fitd_stage_load(FitdStage *stage,const char *directory,
                     unsigned floor,FILE *log){
    if(!stage || !directory || floor>99)return false;
    char path[256];
    snprintf(path,sizeof(path),"%s/ETAGE%02u.PAK",directory,floor);
    FILE *f=fopen(path,"rb");
    if(!f){
        if(log){fprintf(log,"FITD-WII-STAGE missing=%s\n",path);fflush(log);}
        return false;
    }
    FitdPakDirectory dir={0};
    FitdPakEntry room_entry={0},camera_entry={0};
    int st=fitd_pak_directory(f,&dir);
    if(st==0)st=fitd_pak_entry(f,&dir,0,&room_entry);
    if(st==0)st=fitd_pak_entry(f,&dir,1,&camera_entry);
    uint8_t *rooms=NULL,*cams=NULL;
    size_t nr=0,nc=0;
    if(st==0)st=fitd_pak_load(f,&room_entry,&rooms,&nr);
    if(st==0)st=fitd_pak_load(f,&camera_entry,&cams,&nc);
    fclose(f);
    if(st!=0){
        if(log){fprintf(log,"FITD-WII-STAGE floor=%u archive_error=%s\n",
                        floor,fitd_pak_error(st));fflush(log);}
        free(rooms);free(cams);
        return false;
    }
    st=fitd_stage_set_blobs(stage,rooms,nr,cams,nc);
    if(st!=0){
        if(log){fprintf(log,"FITD-WII-STAGE floor=%u parse_error=%d room_bytes=%lu camera_bytes=%lu\n",
                        floor,st,(unsigned long)nr,(unsigned long)nc);fflush(log);}
        free(rooms);free(cams);
        return false;
    }
    stage->floor_number=floor;
    if(log){
        fprintf(log,"FITD-WII-STAGE OK floor=%u rooms=%u cameras=%u "
                "room_bytes=%lu camera_bytes=%lu\n",
                floor,stage->room_count,stage->camera_count,
                (unsigned long)nr,(unsigned long)nc);
        fflush(log);
    }
    return true;
}
static int read_zone_set(const uint8_t *data,size_t start,size_t end,
                         uint16_t off,FitdStageZone *zones,unsigned *count){
    size_t p=start+(size_t)off;
    if(p<start || !fits(end,p,2))return -1;
    const unsigned n=u16(data,p);p+=2;
    if(n>FITD_STAGE_MAX_ZONE_RECORDS || !fits(end,p,n*16u))return -2;
    for(unsigned i=0;i<n;i++){
        FitdStageZone *z=&zones[i];
        z->x1=s16(data,p+0);
        z->x2=s16(data,p+2);
        z->y1=s16(data,p+4);
        z->y2=s16(data,p+6);
        z->z1=s16(data,p+8);
        z->z2=s16(data,p+10);
        z->parameter=u16(data,p+12);
        z->type=u16(data,p+14);
        p+=16;
    }
    *count=n;
    return 0;
}
int fitd_stage_room(const FitdStage *stage,unsigned index,FitdStageRoom *out){
    if(!stage || !stage->valid || !out)return -1;
    size_t p=0,end=0;
    if(blob_span(stage->rooms,stage->rooms_size,stage->room_count,
                 index,&p,&end)!=0 || !fits(end,p,12))return -2;
    const uint8_t *d=stage->rooms;
    memset(out,0,sizeof(*out));
    const uint16_t col=u16(d,p);
    const uint16_t sce=u16(d,p+2);
    out->world_x=s16(d,p+4);
    out->world_y=s16(d,p+6);
    out->world_z=s16(d,p+8);
    const unsigned n=u16(d,p+10);
    if(n==0 || n>FITD_STAGE_MAX_ROOM_CAMERAS ||
       !fits(end,p+12,n*2u))return -3;
    out->camera_count=n;
    out->index=index;
    for(unsigned i=0;i<n;i++){
        out->camera_ids[i]=u16(d,p+12+i*2u);
        if(out->camera_ids[i]>=stage->camera_count)return -4;
    }
    if(read_zone_set(d,p,end,col,out->hard,&out->hard_count)!=0 ||
       read_zone_set(d,p,end,sce,out->scene,&out->scene_count)!=0)return -5;
    out->valid=true;
    return 0;
}
int fitd_stage_camera(const FitdStage *stage,unsigned index,
                      FitdStageCamera *out){
    if(!stage || !stage->valid || !out)return -1;
    size_t p=0,end=0;
    if(blob_span(stage->cameras,stage->cameras_size,
                 stage->camera_count,index,&p,&end)!=0 ||
       !fits(end,p,20))return -2;
    const uint8_t *d=stage->cameras;
    memset(out,0,sizeof(*out));
    out->alpha=s16(d,p);
    out->beta=s16(d,p+2);
    out->gamma=s16(d,p+4);
    out->x=s16(d,p+6);
    out->y=s16(d,p+8);
    out->z=s16(d,p+10);
    for(unsigned k=0;k<3;k++)out->focal[k]=u16(d,p+12+k*2u);
    const unsigned count=u16(d,p+18);
    if(count>FITD_STAGE_MAX_ROOM_CAMERAS ||
       !fits(end,p+20,count*12u))return -3;
    out->index=index;
    out->viewed_count=count;
    out->raw_offset=p;
    out->raw_limit=end;
    for(unsigned i=0;i<count;i++){
        const size_t off=p+20+i*12u;
        FitdStageCameraView *v=&out->viewed[i];
        v->room_index=u16(d,off);
        v->mask_offset=u16(d,off+2);
        v->cover_offset=u16(d,off+4);
        v->light_x=s16(d,off+6);
        v->light_y=s16(d,off+8);
        v->light_z=s16(d,off+10);
    }
    out->valid=true;
    return 0;
}
bool fitd_stage_collision_xz(const FitdStageRoom *r,
                             int x,int y,int z,unsigned *index){
    if(!r || !r->valid)return false;
    for(unsigned i=0;i<r->hard_count;i++){
        const FitdStageZone *b=&r->hard[i];
        if(x>=b->x1 && x<=b->x2 &&
           y>=b->y1 && y<=b->y2 &&
           z>=b->z1 && z<=b->z2){
            if(index)*index=i;
            return true;
        }
    }
    return false;
}
int fitd_stage_raster_polygon(uint8_t *mask,size_t pixels,
                              const int16_t *xy,unsigned points){
    if(!mask || pixels<FITD_STAGE_MASK_PIXELS ||
       !xy || points<3 || points>64)return -1;
    unsigned filled=0;
    int min_y=FITD_STAGE_MASK_H-1;
    int max_y=0;
    for(unsigned i=0;i<points;i++){
        int y=xy[i*2+1];
        if(y<min_y)min_y=y;
        if(y>max_y)max_y=y;
    }
    if(max_y<0 || min_y>=FITD_STAGE_MASK_H)return 0;
    if(min_y<0)min_y=0;
    if(max_y>=FITD_STAGE_MASK_H)max_y=FITD_STAGE_MASK_H-1;
    for(int y=min_y;y<=max_y;y++){
        const float scan=(float)y+0.5f;
        float xx[64];unsigned n=0;
        for(unsigned a=0;a<points;a++){
            unsigned b=(a+1)%points;
            float y1=(float)xy[2*a+1],y2=(float)xy[2*b+1];
            if((y1<=scan && y2>scan)||(y2<=scan && y1>scan)){
                float x1=(float)xy[2*a],x2=(float)xy[2*b];
                float x=x1+(scan-y1)*(x2-x1)/(y2-y1);
                unsigned k=n++;
                while(k>0 && xx[k-1]>x){
                    xx[k]=xx[k-1];k--;
                }
                xx[k]=x;
            }
        }
        for(unsigned k=0;k+1<n;k+=2){
            int x0=(int)ceilf(xx[k]);
            int x1=(int)floorf(xx[k+1]);
            if(x1<0 || x0>=(int)FITD_STAGE_MASK_W)continue;
            if(x0<0)x0=0;
            if(x1>=FITD_STAGE_MASK_W)x1=FITD_STAGE_MASK_W-1;
            for(int x=x0;x<=x1;x++){
                const unsigned idx=(unsigned)y*FITD_STAGE_MASK_W+(unsigned)x;
                if(!mask[idx]){mask[idx]=255;filled++;}
            }
        }
    }
    return (int)filled;
}
int fitd_stage_masks(const FitdStage *stage,unsigned camera_index,
                     int room_index,FitdStageMask *out,FILE *log){
    if(!out)return -1;
    memset(out,0,sizeof(*out));
    if(!stage || !stage->valid)return -2;
    FitdStageCamera cam;
    int rc=fitd_stage_camera(stage,camera_index,&cam);
    if(rc!=0)return rc;
    if(cam.viewed_count==0)return -3;
    unsigned selected=0;
    if(room_index>=0){
        bool found=false;
        for(unsigned i=0;i<cam.viewed_count;i++)
            if(cam.viewed[i].room_index==(unsigned)room_index){
                selected=i;found=true;break;
            }
        if(!found)return -4;
    }
    const FitdStageCameraView *v=&cam.viewed[selected];
    out->viewed_room=(int)v->room_index;
    const uint8_t *d=stage->cameras;
    const size_t end=cam.raw_limit;
    if(v->mask_offset==0)return -5;
    const size_t base=cam.raw_offset+v->mask_offset;
    if(base<cam.raw_offset || !fits(end,base,2))return -6;
    const unsigned masks=u16(d,base);
    if(masks>64)return -7;
    size_t rec=base+2;
    for(unsigned m=0;m<masks;m++){
        if(!fits(end,rec,4))return -8;
        const unsigned rects=u16(d,rec);
        const unsigned poly_offset=u16(d,rec+2);
        if(rects>128 || !fits(end,rec+4,(size_t)rects*8u))
            return -9;
        const size_t poly_base=base+poly_offset;
        if(poly_base<base || !fits(end,poly_base,2))return -10;
        size_t p=poly_base;
        const unsigned polys=u16(d,p);p+=2;
        if(polys>256)return -11;
        for(unsigned polygon=0;polygon<polys;polygon++){
            if(!fits(end,p,2))return -12;
            const unsigned n=u16(d,p);p+=2;
            if(n<3 || n>64 || !fits(end,p,(size_t)n*4u))return -13;
            int16_t points[128];
            for(unsigned j=0;j<n;j++){
                points[j*2]=s16(d,p+j*4u);
                points[j*2+1]=s16(d,p+j*4u+2u);
            }
            int added=fitd_stage_raster_polygon(out->covered,
                                                 sizeof(out->covered),points,n);
            if(added<0)return -14;
            out->covered_pixels+=(unsigned)added;
            out->polygons++;
            p+=(size_t)n*4u;
        }
        rec+=4+(size_t)rects*8u;
        out->masks++;
    }
    out->valid=true;
    if(log){
        fprintf(log,"FITD-WII-MASK05 OK camera=%u room=%d "
                "masks=%u polygons=%u covered_pixels=%u\n",
                camera_index,out->viewed_room,out->masks,
                out->polygons,out->covered_pixels);
        fflush(log);
    }
    return 0;
}
