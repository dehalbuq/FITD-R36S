/*
 * Synthetic BODY regression suite for Wii PowerPC parser.
 * No copyrighted game data is included. Exercise both animation group
 * header formats, polygon indices, malformed/truncated data.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../source/fitd_body.h"
typedef struct {uint8_t bytes[512];size_t n;} Writer;
static void w8(Writer *w,uint8_t x){w->bytes[w->n++]=x;}
static void w16(Writer *w,uint16_t x){
    w8(w,(uint8_t)x);w8(w,(uint8_t)(x>>8));
}
static Writer make_body(unsigned flags,int bad_offset){
    Writer w={{0},0};
    w16(&w,(uint16_t)flags);
    for(int i=0;i<6;i++)w16(&w,0);   /* ZV range */
    w16(&w,0);                        /* scratch len */
    w16(&w,4);                        /* 4 vertices */
    const int16_t v[4][3]={
        {-40,-30,0},{40,-30,0},{40,30,0},{-40,30,0}};
    for(int i=0;i<4;i++)
        for(int k=0;k<3;k++)w16(&w,(uint16_t)v[i][k]);
    if(flags&2){
        w16(&w,1);                     /* one bone */
        w16(&w,0);                     /* bone0 order offset=0 */
        w16(&w,0);                     /* bone vertex byte start */
        w16(&w,4);                     /* 4 bone vertices */
        w16(&w,0);                     /* reference vertex offset */
        w8(&w,0);w8(&w,0);             /* parent / group */
        w16(&w,0);                     /* kind=rotation */
        w16(&w,0);w16(&w,0);w16(&w,0); /* deltas */
        if(flags&8){
            w16(&w,0);w16(&w,0);w16(&w,0);w16(&w,0);
        }
    }
    w16(&w,1);                        /* one primitive */
    w8(&w,1);                         /* polygon */
    w8(&w,4);                         /* 4 sides */
    w8(&w,0);                         /* material */
    w8(&w,101);                       /* palette color */
    w16(&w,0);
    w16(&w,6);
    w16(&w,bad_offset?99:12);
    w16(&w,18);
    return w;
}
static int parse(unsigned flags,int bad_offset,int truncate){
    Writer w=make_body(flags,bad_offset);
    FitdBody *b=(FitdBody*)calloc(1,sizeof(FitdBody));
    if(!b)return 98;
    const int status=fitd_body_parse(w.bytes,
                w.n-(truncate?3:0),b);
    int ok=(bad_offset||truncate)?status!=0:
        (status==0&&b->valid&&b->vertex_count==4&&
         b->primitive_count==1&&b->primitives[0].count==4&&
         b->primitives[0].point[2]==2&&b->scale>0.f &&
         b->group_count==((flags&2)?1:0));
    if(!ok)printf("body test ERROR flags=%u bad=%d trim=%d status=%d\n",
                  flags,bad_offset,truncate,status);
    free(b);
    return ok?0:1;
}
int main(void){
    int err=0;
    err+=parse(0,0,0);
    err+=parse(2,0,0);
    err+=parse(2|8,0,0);
    err+=parse(2,1,0);
    err+=parse(0,0,1);
    if(err){printf("BODY parser tests failed=%d\n",err);return 1;}
    puts("FITD Wii Test03 BODY parser PASS: LE static, ANIM, OPTIMISE, invalid indices, truncated data");
    return 0;
}
