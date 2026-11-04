/*
 * FITD Wii GX bootstrap v0.1 -- Wii homebrew hardware/renderer validation.
 *
 * This is NOT the FITD game engine yet. It exercises native PowerPC,
 * Hollywood GX 3D + Z-buffer, 480p/480i VI, Wii Remote/GameCube inputs,
 * FAT SD storage, and little-endian original-game pack discovery.
 * No copyrighted game code/data is bundled.
 *
 * GPL-2.0-or-later; FITD upstream engine is GPL-2.0.
 */
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <ogc/lwp_watchdog.h>
#include <fat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <malloc.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>
#include <strings.h>
#include "fitd_endian.h"

#define GX_FIFO_BYTES (256u*1024u)
#define DATA_DIR "sd:/apps/fitd-wii/data"
#define LOG_FILE "sd:/apps/fitd-wii/wii-diagnostic.txt"

static GXRModeObj *videoMode;
static void *xfb[2]={NULL,NULL};
static void *gxFifo=NULL;
static Mtx cameraMatrix;
static Mtx44 projection;
static int sdReady=0, pakCount=0;
static unsigned long frameCount=0;
static int storedFrame=0;
static FILE *diag=NULL;

static void record(const char *name,const char *value) {
    if(diag){
        fprintf(diag,"%s=%s\n",name,value);
        fflush(diag);
    }
}
static int has_suffix(const char *filename,const char *suffix) {
    size_t a=strlen(filename),b=strlen(suffix);
    return a>=b && strcasecmp(filename+a-b,suffix)==0;
}
static void inspect_game_data(void){
    DIR *folder=opendir(DATA_DIR);
    if(!folder) {
        puts("Game data: not installed (expected on first boot).");
        record("game_data","not-found");
        return;
    }
    struct dirent *e;
    while((e=readdir(folder))!=NULL){
        if(!has_suffix(e->d_name,".PAK") && !has_suffix(e->d_name,".HQR"))
            continue;
        ++pakCount;
        if(diag && pakCount<=24)fprintf(diag,"pack[%d]=%s\n",pakCount,e->d_name);
    }
    closedir(folder);
    char summary[70];
    snprintf(summary,sizeof(summary),"Packs detected in data/: %d",pakCount);
    puts(summary);
    if(diag){
        fprintf(diag,"game_pack_count=%d\n",pakCount);
        fflush(diag);
    }
}
static void scan_hardware(void){
    puts("FITD Wii / GX prototype v0.1");
    puts("----------------------------------------");
    puts("Native Wii Broadway PowerPC / libogc");
    printf("Framebuffer: %ux%u, EFB height: %u\n",
        videoMode->fbWidth,videoMode->xfbHeight,videoMode->efbHeight);
    printf("TV video mode: 0x%08lx\n",(unsigned long)videoMode->viTVMode);
    printf("CPU endian: %s\n",fitd_is_big_endian()?"big-endian":"little-endian");
    printf("Little-endian parser selftest: %s\n",
        fitd_endian_selftest()?"PASS":"FAIL");
    printf("FAT/SD mount: %s\n",sdReady?"OK":"NOT AVAILABLE");
    inspect_game_data();
    puts("");
    puts("This boot.dol is NOT the playable game yet.");
    puts("GX geometry, controllers and SD are being tested.");
    puts("");
    puts("Wii Remote: A = GX test, HOME = exit");
    puts("GameCube pad: A = GX test, START = exit");
    puts("GX test: D-pad move; B rotate; HOME exit");
    puts("");
    puts("Press A to enter the 3D graphics test.");
    record("build","wii-gx-prototype-0.1");
    record("gx_stage","waiting-input");
}
static void present_console(void){
    VIDEO_SetNextFramebuffer(xfb[0]);
    VIDEO_SetBlack(false);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if(videoMode->viTVMode & VI_NON_INTERLACE)VIDEO_WaitVSync();
}
static bool input_exit(u32 wpDown,u32 gcDown){
    return (wpDown & WPAD_BUTTON_HOME) || (gcDown & PAD_BUTTON_START);
}
static void draw_cube(float x,float y,float z, float sx,float sy,float sz,
                      float yaw,float red,float green,float blue){
    static const float cube[6][4][3]={
        {{-1,-1, 1},{ 1,-1, 1},{ 1, 1, 1},{-1, 1, 1}},
        {{ 1,-1,-1},{-1,-1,-1},{-1, 1,-1},{ 1, 1,-1}},
        {{-1, 1, 1},{ 1, 1, 1},{ 1, 1,-1},{-1, 1,-1}},
        {{-1,-1,-1},{ 1,-1,-1},{ 1,-1, 1},{-1,-1, 1}},
        {{ 1,-1, 1},{ 1,-1,-1},{ 1, 1,-1},{ 1, 1, 1}},
        {{-1,-1,-1},{-1,-1, 1},{-1, 1, 1},{-1, 1,-1}}
    };
    const float light[6]={1.0f,0.42f,0.87f,0.52f,0.74f,0.57f};
    guVector axis={0.f,1.f,0.f};
    Mtx transform,modelView;
    guMtxIdentity(transform);
    guMtxRotAxisDeg(transform,&axis,yaw);
    guMtxTransApply(transform,transform,x,y,z);
    guMtxConcat(cameraMatrix,transform,modelView);
    GX_LoadPosMtxImm(modelView,GX_PNMTX0);
    for(int face=0;face<6;face++){
        GX_Begin(GX_QUADS,GX_VTXFMT0,4);
        for(int vertex=0;vertex<4;vertex++){
            GX_Position3f32(cube[face][vertex][0]*sx,
                           cube[face][vertex][1]*sy,
                           cube[face][vertex][2]*sz);
            GX_Color3f32(red*light[face],
                         green*light[face],
                         blue*light[face]);
        }
        GX_End();
    }
}
static void init_gx(void){
    gxFifo=memalign(32,GX_FIFO_BYTES);
    if(!gxFifo){puts("ERROR: GX FIFO allocation failed");return;}
    memset(gxFifo,0,GX_FIFO_BYTES);
    GX_Init(gxFifo,GX_FIFO_BYTES);
    GXColor background={10,13,20,255};
    GX_SetCopyClear(background,0x00ffffff);
    GX_SetViewport(0,0,videoMode->fbWidth,videoMode->efbHeight,0,1);
    const f32 yscale=GX_GetYScaleFactor(videoMode->efbHeight,videoMode->xfbHeight);
    const u32 copyHeight=GX_SetDispCopyYScale(yscale);
    GX_SetScissor(0,0,videoMode->fbWidth,videoMode->efbHeight);
    GX_SetDispCopySrc(0,0,videoMode->fbWidth,videoMode->efbHeight);
    GX_SetDispCopyDst(videoMode->fbWidth,copyHeight);
    GX_SetCopyFilter(videoMode->aa,videoMode->sample_pattern,GX_TRUE,videoMode->vfilter);
    GX_SetFieldMode(videoMode->field_rendering,
        (videoMode->viHeight==2*videoMode->xfbHeight)?GX_ENABLE:GX_DISABLE);
    GX_SetPixelFmt(videoMode->aa?GX_PF_RGB565_Z16:GX_PF_RGB8_Z24,GX_ZC_LINEAR);
    GX_SetCullMode(GX_CULL_NONE);
    GX_SetZMode(GX_TRUE,GX_LEQUAL,GX_TRUE);
    GX_SetColorUpdate(GX_TRUE);
    GX_SetAlphaUpdate(GX_FALSE);
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS,GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0,GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XYZ,GX_F32,0);
    GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_CLR0,GX_CLR_RGBA,GX_RGB8,0);
    GX_SetNumChans(1);
    GX_SetNumTexGens(0);
    GX_SetNumTevStages(1);
    GX_SetTevOrder(GX_TEVSTAGE0,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0,GX_PASSCLR);
    GX_SetDispCopyGamma(GX_GM_1_0);
    const guVector eye={0.f,1.0f,5.f},up={0.f,1.f,0.f},look={0.f,-0.1f,-6.f};
    guLookAt(cameraMatrix,&eye,&up,&look);
    guPerspective(projection,49.f,
        (f32)videoMode->fbWidth/(f32)videoMode->efbHeight,0.1f,120.f);
    GX_LoadProjectionMtx(projection,GX_PERSPECTIVE);
    GX_CopyDisp(xfb[storedFrame],GX_TRUE);
    record("gx_backend","initialized");
}
static void render_gx(float x,float z,float rotation,float walk){
    GX_SetViewport(0,0,videoMode->fbWidth,videoMode->efbHeight,0,1);
    GX_SetZMode(GX_TRUE,GX_LEQUAL,GX_TRUE);
    // A minimal fixed-function GX scene: floor, walls, foreground obstruction
    // and a low-poly animated actor. Tests Z-order and native 3D transforms.
    draw_cube(0.f,-1.75f,-7.f,5.9f,0.11f,5.7f,0.f,0.22f,0.25f,0.36f);
    draw_cube(0.f,0.7f,-11.f,5.7f,2.5f,0.1f,0.f,0.16f,0.21f,0.30f);
    draw_cube(-5.3f,0.7f,-7.f,0.12f,2.5f,5.f,0.f,0.23f,0.24f,0.32f);
    draw_cube(5.3f,0.7f,-7.f,0.12f,2.5f,5.f,0.f,0.23f,0.24f,0.32f);
    // Use individual low-poly cubes as a visible character skeleton.
    const float depth=-6.f+z;
    draw_cube(x,0.0f,depth,0.48f,0.68f,0.27f,rotation,0.54f,0.68f,0.90f);
    draw_cube(x,1.05f,depth,0.32f,0.32f,0.32f,rotation,0.84f,0.71f,0.62f);
    draw_cube(x-0.65f,-0.02f+walk*0.08f,depth,0.18f,0.55f,0.18f,
        rotation,0.45f,0.61f,0.80f);
    draw_cube(x+0.65f,-0.02f-walk*0.08f,depth,0.18f,0.55f,0.18f,
        rotation,0.45f,0.61f,0.80f);
    draw_cube(x-0.27f,-1.02f-walk*0.15f,depth,0.21f,0.47f,0.21f,
        rotation,0.19f,0.24f,0.39f);
    draw_cube(x+0.27f,-1.02f+walk*0.15f,depth,0.21f,0.47f,0.21f,
        rotation,0.19f,0.24f,0.39f);
    // A fixed foreground column checks GX occlusion against actors.
    draw_cube(2.6f,-0.1f,-3.6f,0.42f,1.65f,0.42f,
        0.f,0.65f,0.49f,0.34f);
    GX_DrawDone();
    storedFrame^=1;
    GX_CopyDisp(xfb[storedFrame],GX_TRUE);
    VIDEO_SetNextFramebuffer(xfb[storedFrame]);
    VIDEO_Flush();
    VIDEO_WaitVSync();
}
int main(int argc,char **argv){
    (void)argc;(void)argv;
    VIDEO_Init();
    WPAD_Init();
    PAD_Init();
    videoMode=VIDEO_GetPreferredMode(NULL);
    if(!videoMode)return 2;
    xfb[0]=MEM_K0_TO_K1(SYS_AllocateFramebuffer(videoMode));
    xfb[1]=MEM_K0_TO_K1(SYS_AllocateFramebuffer(videoMode));
    if(!xfb[0]||!xfb[1])return 3;
    console_init(xfb[0],20,20,videoMode->fbWidth-40,
        videoMode->xfbHeight-40,videoMode->fbWidth*VI_DISPLAY_PIX_SZ);
    VIDEO_Configure(videoMode);
    present_console();
    sdReady=fatInitDefault()?1:0;
    if(sdReady){
        diag=fopen(LOG_FILE,"w");
        if(!diag)diag=fopen("sd:/fitd-wii-diagnostic.txt","w");
        if(diag){
            fprintf(diag,"FITD WII GX bootstrap v0.1\n");
            fprintf(diag,"native_ppc_endian=%s\n",fitd_is_big_endian()?"BE":"LE");
            fprintf(diag,"framebuffer=%ux%u efb=%u\n",
                videoMode->fbWidth,videoMode->xfbHeight,videoMode->efbHeight);
        }
    }
    scan_hardware();
    int stage=0;
    float x=0.f,z=0.f,rotation=0.f,phase=0.f;
    u64 startTicks=gettime();
    while(SYS_MainLoop()){
        WPAD_ScanPads();
        PAD_ScanPads();
        const u32 wpDown=WPAD_ButtonsDown(0);
        const u32 gcDown=PAD_ButtonsDown(0);
        if(input_exit(wpDown,gcDown))break;
        if(stage==0){
            if((wpDown & WPAD_BUTTON_A)||(gcDown & PAD_BUTTON_A)){
                stage=1;
                init_gx();
                if(!gxFifo)break;
                puts("GX initialized.");
                record("stage","gx-active");
                startTicks=gettime();
            }else VIDEO_WaitVSync();
            continue;
        }
        const u32 wpHeld=WPAD_ButtonsHeld(0), gcHeld=PAD_ButtonsHeld(0);
        bool walking=false;
        if((wpHeld&WPAD_BUTTON_LEFT)||(gcHeld&PAD_BUTTON_LEFT)){x-=0.045f;walking=true;}
        if((wpHeld&WPAD_BUTTON_RIGHT)||(gcHeld&PAD_BUTTON_RIGHT)){x+=0.045f;walking=true;}
        if((wpHeld&WPAD_BUTTON_UP)||(gcHeld&PAD_BUTTON_UP)){z-=0.045f;walking=true;}
        if((wpHeld&WPAD_BUTTON_DOWN)||(gcHeld&PAD_BUTTON_DOWN)){z+=0.045f;walking=true;}
        if((wpHeld&WPAD_BUTTON_B)||(gcHeld&PAD_BUTTON_B))rotation+=1.65f;
        if(x>3.5f)x=3.5f;
        if(x< -3.5f)x=-3.5f;
        if(z>2.f)z=2.f;
        if(z< -3.0f)z=-3.0f;
        phase+=walking?0.15f:0.04f;
        const float walk=walking?sinf(phase):0.f;
        render_gx(x,z,rotation,walk);
        ++frameCount;
        if(diag && frameCount%300==0){
            const u64 elapsedMs=ticks_to_millisecs(gettime()-startTicks);
            const float fps=elapsedMs?((float)frameCount*1000.f/elapsedMs):0.f;
            fprintf(diag,"GX frames=%lu fps=%.2f actor=%.2f,%.2f yaw=%.1f\n",
                frameCount,fps,x,z,rotation);
            fflush(diag);
        }
    }
    if(diag){
        fprintf(diag,"finish frames=%lu endianness_check=%s\n",frameCount,
            fitd_endian_selftest()?"PASS":"FAIL");
        fclose(diag);
    }
    if(gxFifo)free(gxFifo);
    return 0;
}
