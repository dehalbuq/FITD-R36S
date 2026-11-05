/* Regression for black-fringe-free bilinear GX mask textures.
 * All RGB samples equal corresponding camera pixels, even where alpha=0.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../source/fitd_mask_texture.h"
static int check_texel(const uint8_t *tile,unsigned w,unsigned x,unsigned y,
                       const uint8_t rgb[3],uint8_t alpha){
    const size_t off=fitd_mask_rgba8_offset(w,x,y);
    return tile[off]==alpha && tile[off+1]==rgb[0] &&
           tile[off+32]==rgb[1] && tile[off+33]==rgb[2];
}
int main(void){
    uint8_t t[8u*8u*4u]={0};
    const uint8_t a[3]={48,110,174},b[3]={224,69,35};
    fitd_mask_rgba8_write(t,8,0,0,0,a);
    fitd_mask_rgba8_write(t,8,1,0,255,a);
    fitd_mask_rgba8_write(t,8,3,3,255,b);
    fitd_mask_rgba8_write(t,8,4,0,0,b);
    fitd_mask_rgba8_write(t,8,0,4,255,a);
    fitd_mask_rgba8_write(t,8,7,7,0,b);
    if(fitd_mask_rgba8_offset(8,0,0)!=0 ||
       fitd_mask_rgba8_offset(8,3,3)!=30 ||
       fitd_mask_rgba8_offset(8,4,0)!=64 ||
       fitd_mask_rgba8_offset(8,0,4)!=128 ||
       !check_texel(t,8,0,0,a,0) ||
       !check_texel(t,8,1,0,a,255) ||
       !check_texel(t,8,3,3,b,255) ||
       !check_texel(t,8,4,0,b,0) ||
       !check_texel(t,8,0,4,a,255) ||
       !check_texel(t,8,7,7,b,0)){
        puts("FAILED tiled mask RGBA8 alpha and RGB preservation");
        return 1;
    }
    puts("PASS tiled RGBA8 alpha with full camera RGB on transparent texels");
    return 0;
}
