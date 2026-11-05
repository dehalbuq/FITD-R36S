/*
 * FITD Wii GX original ETAGE actor articulated rest pose, seamless bilinear masks and clip inspection v0.7.0 -- Wii homebrew hardware/renderer validation.
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
#include <errno.h>
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
#include "fitd_pak.h"
#include "fitd_camera.h"
#include "fitd_body.h"
#include "fitd_anim.h"
#include "fitd_stage.h"
#include "fitd_mask_gx.h"
#include "fitd_scene.h"
#include "fitd_scene_gx.h"
#include "fitd_motion.h"
#include "fitd_suite.h"
#include "fitd_hud.h"

#define GX_FIFO_BYTES (256u*1024u)
#define DATA_DIR "sd:/apps/fitd-wii/data"
#define LOG_FILE "sd:/apps/fitd-wii/wii-diagnostic.txt"
#define ROOT_LOG_FILE "sd:/fitd-wii-test07-batch-diagnostic.txt"

static GXRModeObj *videoMode;
static void *xfb[2]={NULL,NULL};
static void *gxFifo=NULL;
static Mtx cameraMatrix;
static Mtx44 projection;
static int sdReady=0, pakCount=0;
static FitdCamera originalCamera={0};
static bool showOriginalCamera=false;
static FitdBody originalBody={0};
static FitdAnimInfo animMetadata={0};
static bool showOriginalBody=false;
static FitdAnimClip originalAnimation={0};
/* Only animations matching BODY 15 can be previewed on command. */
#define BODY_CANDIDATE_CAP 128u
static unsigned actorCandidates[BODY_CANDIDATE_CAP]={0};
static unsigned actorCandidateCount=0;
static int actorCandidateSlot=-1; /* -1 = articulated BODY rest pose */
static FitdAnimClip actorCandidateAnimation={0};
static bool actorRestReady=false;
static double actorCandidateTicks=0.0;
static FitdMotionController actorMotion={0};
static unsigned displayedAnim=0;
static FitdPoseVertex animationPose[FITD_BODY_MAX_VERTICES];
static FitdAnimSample animationSample={0};
static bool animationPlaying=true;
static double animationTicks=0.0;
static u64 previousAnimTime=0;
static bool poseReady=false;
static FILE *diag=NULL;
static FitdStage originalStage={0};
static FitdStageRoom currentStageRoom={0};
static FitdStageMask stageMasks={0};
static FitdMaskOverlay stageOverlay={0};
static bool showStageMask=false;
static int selectedMaskIndex=-1; /* -1 means union for legacy inspection */
static unsigned stageMaskRefreshes=0;
static int stageMaskStatus=-1;
static bool actorSceneMode=false;
static FitdSceneActor sceneActor={0};
static FitdStageCamera sceneCamera={0};
static unsigned sceneProjected=0,sceneMaskSelected=0;
static unsigned sceneCamChanges=0,sceneMaskChecks=0;
/* Automated Test07 diagnostics do not modify the real actor or game assets. */
#define GALLERY_PERIOD_MS 2200u
static FitdSuiteReport suite07={0};
static bool gallery07=false;
static unsigned gallery07Slot=0,gallery07Elapsed=0;
static unsigned gallery07Started=0,gallery07Finished=0,gallery07Bad=0;



/* A gallery entry is always resolved against the 32 already validated
 * animation indices for this BODY. No unknown clips are applied. */
static bool actor_use_candidate(unsigned index){
    fitd_anim_release(&actorCandidateAnimation);
    actorCandidateSlot=-1;
    actorCandidateTicks=0.0;
    for(unsigned i=0;i<actorCandidateCount;i++){
        if(actorCandidates[i]==index){actorCandidateSlot=(int)i;break;}
    }
    if(actorCandidateSlot<0)return false;
    if(!fitd_anim_load(&actorCandidateAnimation,DATA_DIR,index,diag) ||
       actorCandidateAnimation.group_count!=originalBody.group_count ||
       !fitd_anim_can_apply(&actorCandidateAnimation,&originalBody)){
        fitd_anim_release(&actorCandidateAnimation);
        actorCandidateSlot=-1;
        return false;
    }
    return true;
}
static void gallery07_stop(const char *reason){
    const bool was=gallery07;
    gallery07=false;
    gallery07Elapsed=0;
    if(was)gallery07Finished++;
    fitd_anim_release(&actorCandidateAnimation);
    actorCandidateSlot=-1;
    actorCandidateTicks=0.0;
    if(was && diag){
        fprintf(diag,
          "FITD-WII-GALLERY07 finish reason=%s last_slot=%u "
          "started=%u finished=%u errors=%u\\n",
          reason?reason:"unknown",gallery07Slot,gallery07Started,
          gallery07Finished,gallery07Bad);
        fflush(diag);
    }
}
static void gallery07_start(void){
    if(!actorSceneMode || suite07.top_count==0)return;
    gallery07_stop("restart");
    gallery07=true;
    gallery07Slot=0;
    gallery07Elapsed=0;
    gallery07Started++;
    if(!actor_use_candidate(suite07.top_index[0])){
        gallery07Bad++;
        gallery07_stop("load-error");
        return;
    }
    if(diag){
        fprintf(diag,"FITD-WII-GALLERY07 start count=%u ms_per_clip=%u "
                     "index=%u score=%.1f\\n",
                suite07.top_count,GALLERY_PERIOD_MS,
                suite07.top_index[0],suite07.top_score[0]);
        fflush(diag);
    }
}
static void gallery07_step(unsigned delta){
    if(!gallery07 || delta>250u)return;
    gallery07Elapsed+=delta;
    /* Scan a full animation cycle during each preview, instead of
       requiring the user to wait for clips with long original durations.
       This does not alter normal gameplay's 50Hz time. */
    if(actorCandidateAnimation.valid)
        actorCandidateTicks+=(double)actorCandidateAnimation.total_ticks*
                             (double)delta/(double)GALLERY_PERIOD_MS;
    if(gallery07Elapsed<GALLERY_PERIOD_MS)return;
    gallery07Elapsed-=GALLERY_PERIOD_MS;
    gallery07Slot++;
    if(gallery07Slot>=suite07.top_count){
        gallery07_stop("complete");
        return;
    }
    const unsigned index=suite07.top_index[gallery07Slot];
    if(!actor_use_candidate(index)){
        gallery07Bad++;
        gallery07_stop("invalid-clip");
        return;
    }
    if(diag){
        fprintf(diag,
          "FITD-WII-GALLERY07 show=%u/%u index=%u score=%.1f\\n",
          gallery07Slot+1u,suite07.top_count,
          index,suite07.top_score[gallery07Slot]);
        fflush(diag);
    }
}

static void stage_refresh_camera(void){
    stageOverlay.ready=false;
    if(!originalStage.valid || !originalCamera.valid ||
       !originalCamera.raw_pixels || !stageOverlay.tiled_pixels)return;
    FitdStageCamera cam={0};
    if(fitd_stage_camera(&originalStage,originalCamera.current,&cam)!=0){
        if(diag)fprintf(diag,
          "FITD-WII-STAGE05 camera=%u invalid_record\n",
          originalCamera.current);
        return;
    }
    sceneCamera=cam;
    if(diag){
        fprintf(diag,
          "FITD-WII-CAMERA05 index=%u angle=%d,%d,%d "
          "position=%d,%d,%d focal=%u,%u,%u rooms=%u\n",
          cam.index,cam.alpha,cam.beta,cam.gamma,cam.x,cam.y,cam.z,
          cam.focal[0],cam.focal[1],cam.focal[2],cam.viewed_count);
        fflush(diag);
    }
    const int chosenRoom=currentStageRoom.valid?
                         (int)currentStageRoom.index:-1;
    if(actorSceneMode && currentStageRoom.valid){
        int x1=0,x2=0,z1=0,z2=0;
        fitd_scene_bounds(&sceneActor,&x1,&x2,&z1,&z2);
        stageMaskStatus=fitd_stage_masks_actor(&originalStage,cam.index,
                            chosenRoom,x1,x2,z1,z2,
                            &stageMasks,NULL,&sceneMaskSelected);
        sceneMaskChecks++;
    }else{
        sceneMaskSelected=0;
        stageMaskStatus=fitd_stage_masks_selected(&originalStage,cam.index,
                               chosenRoom,selectedMaskIndex,&stageMasks,diag);
    }
    if(stageMaskStatus==-4){
        if(actorSceneMode){
            int x1=0,x2=0,z1=0,z2=0;
            fitd_scene_bounds(&sceneActor,&x1,&x2,&z1,&z2);
            stageMaskStatus=fitd_stage_masks_actor(&originalStage,cam.index,
                -1,x1,x2,z1,z2,&stageMasks,NULL,&sceneMaskSelected);
        }else{
            stageMaskStatus=fitd_stage_masks_selected(&originalStage,cam.index,
                               -1,selectedMaskIndex,&stageMasks,diag);
        }
    }
    if(!actorSceneMode && stageMaskStatus==-15 && selectedMaskIndex>=0){
        /* Another camera can have fewer masks. Fall back to the safe
           union view instead of keeping an invalid mask index. */
        selectedMaskIndex=-1;
        stageMaskStatus=fitd_stage_masks_selected(&originalStage,cam.index,
                           chosenRoom,selectedMaskIndex,&stageMasks,diag);
        if(stageMaskStatus==-4)
            stageMaskStatus=fitd_stage_masks_selected(&originalStage,cam.index,
                           -1,selectedMaskIndex,&stageMasks,diag);
    }
    if(stageMaskStatus==0){
        if(!fitd_mask_gx_update(&stageOverlay,&stageMasks,
                                originalCamera.raw_pixels)){
            if(stageMasks.covered_pixels>0)stageMaskStatus=-20;
        }else{
            stageMaskRefreshes++;
        }
    }
    if(diag){
        fprintf(diag,
          "FITD-WII-MASK05 camera=%u room=%d status=%d "
          "ready=%d masks=%u polys=%u pixels=%u selected=%d "
          "actor_auto=%d actor_matched=%u\n",
          originalCamera.current,stageMasks.viewed_room,
          stageMaskStatus,stageOverlay.ready?1:0,
          stageMasks.masks,stageMasks.polygons,stageMasks.covered_pixels,
          selectedMaskIndex,actorSceneMode?1:0,sceneMaskSelected);
        fflush(diag);
    }
}
static void stage_change_room(int direction){
    if(!originalStage.valid || !originalStage.room_count)return;
    unsigned start=currentStageRoom.valid?currentStageRoom.index:0u;
    for(unsigned delta=1;delta<=originalStage.room_count;delta++){
        unsigned candidate=(start+originalStage.room_count+
                           (unsigned)(direction>0?delta:
                           originalStage.room_count-(delta%originalStage.room_count)))%
                           originalStage.room_count;
        FitdStageRoom next={0};
        if(fitd_stage_room(&originalStage,candidate,&next)!=0)continue;
        currentStageRoom=next;
        if(actorSceneMode){
            fitd_scene_choose_spawn(&sceneActor,&originalStage,
                                    &currentStageRoom,
                                    next.camera_ids[0],diag);
        }
        if(diag){
            fprintf(diag,
              "FITD-WII-ROOM05 selected=%u cameras=%u "
              "pos=%d,%d,%d hard=%u sce=%u\n",
              candidate,next.camera_count,next.world_x,next.world_y,
              next.world_z,next.hard_count,next.scene_count);
            fflush(diag);
        }
        for(unsigned c=0;c<next.camera_count;c++){
            if(next.camera_ids[c]<originalCamera.frames){
                if(fitd_camera_set(&originalCamera,DATA_DIR,
                                   next.camera_ids[c],diag)){
                    stage_refresh_camera();
                    return;
                }
            }
        }
        stage_refresh_camera();
        return;
    }
}
static unsigned long frameCount=0;
static int storedFrame=0;

static const char *activeLogPath=NULL;
static int firstLogError=0;

/*
 * Check that SD is WRITEABLE, rather than assuming fatInitDefault() means
 * write access. Prefer a clearly named diagnostic at SD root, which is
 * easy to locate even if Homebrew uses a different app working folder.
 * Closing/reopening once immediately forces FAT directory metadata
 * to be committed before entering GX graphics mode.
 */
static void diagnostic_open(void){
    if(!sdReady){firstLogError=ENODEV;return;}
    const char *candidates[]={ROOT_LOG_FILE,LOG_FILE};
    for(unsigned i=0;i<2;i++){
        const char *path=candidates[i];
        FILE *f=fopen(path,"wb");
        if(!f){firstLogError=errno;continue;}
        const int bad=fputs("FITD Wii Test07 - log created at boot\n",f)==EOF
                    || fflush(f)!=0 || ferror(f);
        const int closed=fclose(f);
        if(bad || closed!=0){firstLogError=errno;continue;}
        f=fopen(path,"ab");
        if(!f){firstLogError=errno;continue;}
        diag=f;
        activeLogPath=path;
        return;
    }
}
static void diagnostic_checkpoint(void){
    if(!diag || !activeLogPath)return;
    /* fflush() alone does not always update the FAT file length after
       an abrupt Homebrew Channel exit. Close/reopen every 300 frames. */
    fflush(diag);
    fclose(diag);
    diag=fopen(activeLogPath,"ab");
    if(!diag)firstLogError=errno;
}
static void diagnostic_mirror_to_app(void){
    /* Keep legacy path available for users who look inside apps/fitd-wii;
       root log stays authoritative if mirror cannot be written. */
    if(!activeLogPath || strcmp(activeLogPath,LOG_FILE)==0)return;
    FILE *src=fopen(activeLogPath,"rb");
    if(!src)return;
    FILE *out=fopen(LOG_FILE,"wb");
    if(!out){fclose(src);return;}
    unsigned char block[1024];
    size_t count;
    while((count=fread(block,1,sizeof(block),src))>0){
        if(fwrite(block,1,count,out)!=count)break;
    }
    fflush(out);
    fclose(out);
    fclose(src);
}

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
    puts("FITD Wii / GX Teste 07 (automatic hardware suite)");
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
    if(diag && activeLogPath){
        printf("DIAGNOSTIC LOG: %s\n",activeLogPath);
        puts("SD write test: PASS (file created at boot)");
        record("FITD-WII-LOG-PATH",activeLogPath);
    }else{
        printf("WARNING: DIAGNOSTIC NOT SAVED (errno=%d)\n",firstLogError);
        puts("SD read works, but file creation failed.");
        puts("Check SD write protection and free space.");
    }
    puts("");
    puts("GX ETAGE original rooms/cameras/masks + animations.");
    puts("NOT yet playable; actor-world scripts not linked.");
    puts("");
    puts("Wii A / GC A: enter camera and GX tests");
    puts("Wii +/- / GC L/R: change CAMERA00.PAK frame");
    puts("D-pad LEFT/RIGHT: choose BODY model");
    puts("D-pad UP/DOWN: choose LISTANIM entry");
    puts("A: pause/resume keyframe animation (in GX)");
    puts("2 / GC X: toggle BODY; 1 / GC Y: GX demo");
    puts("+/- / GC R/L: switch camera; B: rotate model");
    puts("ACTOR MODE: D-pad move, B turn actor");
    puts("TEST07: ALL 32 animations and cameras scanned at boot");
    puts("B+1 (GC B+Y): auto gallery of 8 top moving clips");
    puts("Big digits show original animation ID; bar is progress");
    puts("B+A: choose any of 32 compatible clips manually");
    puts("Auto camera and masks using original ETAGE zones");
    puts("2 / GC X: switch ACTOR / Test05.3 inspector");
    puts("1 / GC Y: mask on/off in actor mode");
    puts("Inspector: D-pad BODY/ANIM, +/- cameras");
    puts("HOME / GC START: exit");
    puts("HOME / GC START: exit");
    puts("");
    puts("Press A. Without data: original GX demo.");
    record("build","wii-integrated-hardware-suite-test07");
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
    if(showOriginalCamera && originalCamera.valid){
        fitd_camera_draw(&originalCamera,videoMode->fbWidth,videoMode->efbHeight);
        if(showOriginalBody && originalBody.valid){
            if(actorSceneMode){
                /* Test06.2: 287/288 have 18 groups; BODY 15 has 20.
                   Never apply incompatible clips or raw BODY vertices. */
                const bool canPreview=actorCandidateSlot>=0 &&
                    actorCandidateAnimation.valid &&
                    actorCandidateAnimation.group_count==originalBody.group_count &&
                    fitd_anim_can_apply(&actorCandidateAnimation,&originalBody);
                poseReady=canPreview &&
                    fitd_anim_pose_actor(&actorCandidateAnimation,&originalBody,
                        actorCandidateTicks,animationPose,
                        FITD_BODY_MAX_VERTICES,&animationSample);
                displayedAnim=poseReady?actorCandidateAnimation.index:0u;
                if(!poseReady && actorRestReady){
                    poseReady=fitd_anim_pose_rest_actor(&originalBody,
                        animationPose,FITD_BODY_MAX_VERTICES,&animationSample);
                    displayedAnim=0u;
                }
            }else{
                poseReady=fitd_anim_pose(&originalAnimation,&originalBody,
                    animationTicks,animationPose,FITD_BODY_MAX_VERTICES,
                    &animationSample);
                displayedAnim=poseReady?originalAnimation.index:0u;
            }
            if(actorSceneMode && !poseReady){
                /* Fail closed: never draw deformed, unassembled local points. */
                sceneProjected=0;
            }else if(actorSceneMode && sceneCamera.valid && currentStageRoom.valid){
                fitd_scene_gx_draw(&originalBody,
                    poseReady?animationPose:NULL,
                    &currentStageRoom,&sceneCamera,&sceneActor,
                    videoMode->fbWidth,videoMode->efbHeight,
                    &sceneProjected);
            }else{
                fitd_body_draw(&originalBody,cameraMatrix,projection,rotation,
                    x,z,poseReady?animationPose:NULL);
            }
        }else{
            poseReady=false;
            sceneProjected=0;
        }
        if(showStageMask && stageOverlay.ready)
            fitd_mask_gx_draw(&stageOverlay,
                              videoMode->fbWidth,videoMode->efbHeight);
        /* Numeric scene overlay identifies clips during the automated
           run; suppressed during normal REST/movement gameplay. */
        if(actorSceneMode && actorCandidateSlot>=0 &&
           actorCandidateAnimation.valid)
            fitd_hud_draw(actorCandidateAnimation.index,
              gallery07?gallery07Slot:0u, gallery07?suite07.top_count:0u,
              videoMode->fbWidth,videoMode->efbHeight,gallery07);
        GX_DrawDone();
        storedFrame^=1;
        GX_CopyDisp(xfb[storedFrame],GX_TRUE);
        VIDEO_SetNextFramebuffer(xfb[storedFrame]);
        VIDEO_Flush();
        VIDEO_WaitVSync();
        return;
    }
    GX_LoadProjectionMtx(projection,GX_PERSPECTIVE);
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
    diagnostic_open();
    if(diag){
        fprintf(diag,"FITD WII GX rest-pose and seamless bilinear masks v0.7.0\n");
        fprintf(diag,"log_path=%s\n",activeLogPath);
        fprintf(diag,"native_ppc_endian=%s\n",fitd_is_big_endian()?"BE":"LE");
        fprintf(diag,"framebuffer=%ux%u efb=%u\n",
                videoMode->fbWidth,videoMode->xfbHeight,videoMode->efbHeight);
        fflush(diag);
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
                if(sdReady && fitd_camera_open(&originalCamera,DATA_DIR,diag)){
                    showOriginalCamera=true;
                    if(diag)fprintf(diag,
                        "FITD-WII-PREVIEW active camera=0 frames=%u palette=%s\n",
                        originalCamera.frames,
                        originalCamera.correct_palette?"original":"grayscale");
                } else {
                    record("FITD-WII-PREVIEW","unavailable - showing 3D demo");
                }
                if(sdReady){
                    // Search only first 32 records for a valid initial model;
                    // all subsequent indices remain user-selectable.
                    for(unsigned index=0;index<32;index++){
                        if(fitd_body_load(&originalBody,DATA_DIR,index,diag)){
                            showOriginalBody=true;
                            break;
                        }
                    }
                    fitd_animation_info(&animMetadata,DATA_DIR,0,diag);
                    if(fitd_anim_load(&originalAnimation,DATA_DIR,0,diag)){
                        animationPlaying=true;
                        animationTicks=0.0;
                    }
                    if(diag){
                        fprintf(diag,
                          "FITD-WII-TEST05 model_preview=%s "
                          "animation_decoder=%s compatible=%d\n",
                          originalBody.valid?"ACTIVE":"UNAVAILABLE",
                          originalAnimation.valid?"ACTIVE":"UNAVAILABLE",
                          fitd_anim_can_apply(&originalAnimation,&originalBody)?1:0);
                        fflush(diag);
                    }
                    if(fitd_stage_load(&originalStage,DATA_DIR,0,diag)){
                        if(fitd_stage_room(&originalStage,0,&currentStageRoom)==0){
                            if(diag){
                                fprintf(diag,
                                  "FITD-WII-ROOM05 index=0 cams=%u "
                                  "pos=%d,%d,%d hard=%u sce=%u\n",
                                  currentStageRoom.camera_count,
                                  currentStageRoom.world_x,currentStageRoom.world_y,
                                  currentStageRoom.world_z,
                                  currentStageRoom.hard_count,
                                  currentStageRoom.scene_count);
                                fflush(diag);
                            }
                        }
                        /* This BODY/ANIM pair was validated in the Test04 Wii log.
                           Still a manual prototype actor, not the game spawn. */
                        const bool actorBody=
                            fitd_body_load(&originalBody,DATA_DIR,15,diag);
                        /* Test06.1's injury clip remains inspector-only.
                           No unverified idle or walk is assigned. */
                        const bool injuryPreview=
                            fitd_anim_load(&originalAnimation,DATA_DIR,301,diag);
                        actorRestReady=actorBody &&
                            fitd_anim_pose_rest_actor(&originalBody,
                                animationPose,FITD_BODY_MAX_VERTICES,
                                &animationSample);
                        actorCandidateCount=actorBody?
                            fitd_anim_scan_body(DATA_DIR,&originalBody,
                                actorCandidates,BODY_CANDIDATE_CAP,diag):0u;
                        actorCandidateSlot=-1;
                        actorCandidateTicks=0.0;
                        fitd_anim_release(&actorCandidateAnimation);
                        fitd_motion_reset(&actorMotion);
                        actorSceneMode=originalCamera.valid &&
                            currentStageRoom.valid && actorBody;
                        if(diag){
                            fprintf(diag,
                              "FITD-WII-REST06-3 ready=%d body=%u groups=%u "
                              "candidates=%u animation301=%d rejected287_288=1\n",
                              actorRestReady?1:0,originalBody.index,
                              originalBody.group_count,actorCandidateCount,
                              injuryPreview?1:0);
                            fflush(diag);
                        }
                        if(actorSceneMode){
                            fitd_scene_choose_spawn(&sceneActor,&originalStage,
                                &currentStageRoom,originalCamera.current,diag);
                            animationTicks=0.0;
                            showOriginalBody=true;
                            showOriginalCamera=true;
                            showStageMask=true;
                            if(diag){
                                fprintf(diag,
                                  "FITD-WII-ACTOR06 enabled=1 body=%u "
                                  "rest_ready=%d scan_candidates=%u "
                                  "injury_preview=%u mask_auto=1\n",
                                  originalBody.index,actorRestReady?1:0,
                                  actorCandidateCount,originalAnimation.index);
                                fflush(diag);
                            }
                        }
                        if(fitd_mask_gx_init(&stageOverlay))
                            stage_refresh_camera();
                        /* Automatically probe all matching clips, masks,
                           cameras and collision movement on a COPY of
                           the actor. This is a one-time SD asset test. */
                        if(actorSceneMode)
                            fitd_suite_run(DATA_DIR,&originalBody,
                                &originalStage,&currentStageRoom,&sceneActor,
                                actorCandidates,actorCandidateCount,
                                &suite07,diag);
                        else if(diag)
                            fputs("FITD-WII-MASK05 GX_TEXTURE_OOM\n",diag);
                    }
                }
                startTicks=gettime();
                previousAnimTime=startTicks;
            }else VIDEO_WaitVSync();
            continue;
        }
        const u32 wpHeld=WPAD_ButtonsHeld(0), gcHeld=PAD_ButtonsHeld(0);
        const bool bHeld=((wpHeld&WPAD_BUTTON_B)!=0) ||
                         ((gcHeld&PAD_BUTTON_B)!=0);
        const bool modeToggle=(((wpDown&WPAD_BUTTON_2)!=0) ||
                               ((gcDown&PAD_BUTTON_X)!=0)) &&
                              !bHeld && originalStage.valid;
        if(modeToggle){
            gallery07_stop("mode-change");
            actorSceneMode=!actorSceneMode;
            if(actorSceneMode){
                fitd_motion_reset(&actorMotion);
                actorCandidateSlot=-1;
                actorCandidateTicks=0.0;
                fitd_anim_release(&actorCandidateAnimation);
                showOriginalCamera=true;
                showOriginalBody=true;
                showStageMask=true;
            }else{
                showStageMask=false;
            }
            stage_refresh_camera();
            if(diag){
                fprintf(diag,
                    "FITD-WII-ACTOR06 mode=%s camera=%u x=%.0f z=%.0f\n",
                    actorSceneMode?"ACTOR":"INSPECTOR",
                    originalCamera.current,sceneActor.x,sceneActor.z);
                fflush(diag);
            }
        }
        const u64 nowAnimTime=gettime();
        const u64 passedMs=ticks_to_millisecs(nowAnimTime-previousAnimTime);
        previousAnimTime=nowAnimTime;
        /* 50 original-game timing ticks per second; VSync stays near 60.
           No speed-up or animation dependence on the display FPS. */
        if(animationPlaying && passedMs<=250)
            animationTicks+=(double)passedMs*0.050;
        const bool pressedA=((wpDown&WPAD_BUTTON_A)!=0) ||
                            ((gcDown&PAD_BUTTON_A)!=0);
        /* B+A in actor mode cycles structurally compatible clips.
           After the final candidate, returns to BODY rest. */
        if(pressedA && actorSceneMode && bHeld){
            const int next=actorCandidateSlot+1;
            actorCandidateSlot=(unsigned)next<actorCandidateCount?next:-1;
            fitd_anim_release(&actorCandidateAnimation);
            actorCandidateTicks=0.0;
            bool loaded=false;
            if(actorCandidateSlot>=0){
                loaded=fitd_anim_load(&actorCandidateAnimation,DATA_DIR,
                    actorCandidates[actorCandidateSlot],diag);
                if(!loaded ||
                   actorCandidateAnimation.group_count!=originalBody.group_count ||
                   !fitd_anim_can_apply(&actorCandidateAnimation,&originalBody)){
                    fitd_anim_release(&actorCandidateAnimation);
                    actorCandidateSlot=-1;
                    loaded=false;
                }
            }
            if(diag){
                fprintf(diag,
                  "FITD-WII-SCAN06-3 inspect=%s index=%u slot=%d/%u compatible=%d\n",
                  actorCandidateSlot>=0?"CANDIDATE":"REST",
                  loaded?actorCandidateAnimation.index:0u,
                  actorCandidateSlot,actorCandidateCount,loaded?1:0);
                fflush(diag);
            }
        }else if(pressedA){
            animationPlaying=!animationPlaying;
            if(diag){
                fprintf(diag,"FITD-WII-ANIM04 playing=%d ticks=%.2f\n",
                        animationPlaying?1:0,animationTicks);
                fflush(diag);
            }
        }
        if(actorSceneMode && actorCandidateSlot>=0 &&
           animationPlaying && passedMs<=250u)
            actorCandidateTicks+=(double)passedMs*0.05;
        if(!actorSceneMode && showOriginalCamera && originalAnimation.valid){
            int direction=0;
            if((wpDown&WPAD_BUTTON_UP)||(gcDown&PAD_BUTTON_UP))direction=1;
            if((wpDown&WPAD_BUTTON_DOWN)||(gcDown&PAD_BUTTON_DOWN))direction=-1;
            if(direction && originalAnimation.archive_count){
                unsigned next=(originalAnimation.index+
                    (direction>0?1u:originalAnimation.archive_count-1u))%
                    originalAnimation.archive_count;
                if(fitd_anim_load(&originalAnimation,DATA_DIR,next,diag)){
                    animationTicks=0.0;
                    if(diag){
                        fprintf(diag,
                          "FITD-WII-ANIM04 select=%u compatible=%d body=%u\n",
                          next,fitd_anim_can_apply(
                              &originalAnimation,&originalBody)?1:0,
                          originalBody.index);
                        fflush(diag);
                    }
                }
            }
        }
        if(originalCamera.valid){
            const bool combo=((wpHeld&WPAD_BUTTON_B)!=0) ||
                             ((gcHeld&PAD_BUTTON_B)!=0);
            if((wpDown & WPAD_BUTTON_1)||(gcDown & PAD_BUTTON_Y)){
                if(actorSceneMode){
                    showStageMask=!showStageMask;
                    if(diag){
                        fprintf(diag,
                          "FITD-WII-ACTOR06 occlusion=%d camera=%u "
                          "matched=%u pixels=%u\n",
                          showStageMask?1:0,originalCamera.current,
                          sceneMaskSelected,stageMasks.covered_pixels);
                        fflush(diag);
                    }
                }else if(combo){
                    showStageMask=!showStageMask;
                    if(diag){
                        fprintf(diag,
                          "FITD-WII-MASK05 toggled=%d available=%d "
                          "pixels=%u\n",
                          showStageMask?1:0,stageOverlay.ready?1:0,
                          stageMasks.covered_pixels);
                        fflush(diag);
                    }
                }else{
                    /* One-key visibility for the new GPU layer:
                       original camera unmasked -> masked -> GX demo ->
                       original camera unmasked. No hard-to-press chord
                       is required to verify actual mask rendering. */
                    if(!showOriginalCamera){
                        showOriginalCamera=true;
                        showStageMask=false;
                    }else if(!showStageMask){
                        showStageMask=true;
                    }else{
                        showOriginalCamera=false;
                        showStageMask=false;
                    }
                    if(diag){
                        fprintf(diag,
                          "FITD-WII-PREVIEW camera=%d mask_preview=%d "
                          "selected=%d ready=%d\n",
                          showOriginalCamera?1:0,showStageMask?1:0,
                          selectedMaskIndex,stageOverlay.ready?1:0);
                        fflush(diag);
                    }
                }
            }
            int camDirection=0;
            if((wpDown & WPAD_BUTTON_PLUS)||(gcDown & PAD_TRIGGER_R))
                camDirection=1;
            if((wpDown & WPAD_BUTTON_MINUS)||(gcDown & PAD_TRIGGER_L))
                camDirection=-1;
            if(camDirection){
                if(combo && originalStage.valid){
                    stage_change_room(camDirection);
                }else{
                    const unsigned next=(originalCamera.current+
                         (camDirection>0?1u:originalCamera.frames-1u))%
                         originalCamera.frames;
                    if(fitd_camera_set(&originalCamera,DATA_DIR,next,diag)){
                        showOriginalCamera=true;
                        stage_refresh_camera();
                    }else if(diag){
                        fprintf(diag,
                          "FITD-WII-CAMERA frame=%u failed; previous preserved\n",
                          next);
                    }
                }
            }
        }
        if((wpDown&WPAD_BUTTON_2)||(gcDown&PAD_BUTTON_X)){
            const bool maskCombo=((wpHeld&WPAD_BUTTON_B)!=0) ||
                                 ((gcHeld&PAD_BUTTON_B)!=0);
            if(maskCombo && originalStage.valid && stageMasks.masks){
                if(selectedMaskIndex<0)
                    selectedMaskIndex=0;
                else if((unsigned)(selectedMaskIndex+1)>=stageMasks.masks)
                    selectedMaskIndex=-1;
                else
                    selectedMaskIndex++;
                showOriginalCamera=true;
                showStageMask=true;
                stage_refresh_camera();
                if(diag){
                    fprintf(diag,
                      "FITD-WII-MASK05 select=%d total=%u "
                      "pixels=%u status=%d ready=%d\n",
                      selectedMaskIndex,stageMasks.masks,
                      stageMasks.covered_pixels,stageMaskStatus,
                      stageOverlay.ready?1:0);
                    fflush(diag);
                }
            }else if(!modeToggle && originalBody.valid){
                showOriginalBody=!showOriginalBody;
                if(diag)fprintf(diag,"FITD-WII-BODY overlay=%d\n",
                                showOriginalBody?1:0);
            }
        }
        if(originalBody.valid){
            if(!actorSceneMode && showOriginalCamera && ((wpDown&(WPAD_BUTTON_LEFT|WPAD_BUTTON_RIGHT)) ||
                                      (gcDown&(PAD_BUTTON_LEFT|PAD_BUTTON_RIGHT)))){
                const bool forward=((wpDown&WPAD_BUTTON_RIGHT) ||
                                    (gcDown&PAD_BUTTON_RIGHT))!=0;
                const unsigned next=forward
                    ?(originalBody.index+1u)%originalBody.archive_count
                    :(originalBody.index+originalBody.archive_count-1u)
                        %originalBody.archive_count;
                if(fitd_body_load(&originalBody,DATA_DIR,next,diag))
                    showOriginalBody=true;
            }
        }
        /* Real room units, tested against ETAGE hard collision boxes.
           Recompute polygons at most five times per second while walking
           to avoid SD/log overhead on Wii. */
        bool actorMovedThisFrame=false;
        if(actorSceneMode && currentStageRoom.valid && originalCamera.valid){
            float dx=0.f,dz=0.f;
            const float units=35.f;
            if((wpHeld&WPAD_BUTTON_LEFT)||(gcHeld&PAD_BUTTON_LEFT))
                dx-=units;
            if((wpHeld&WPAD_BUTTON_RIGHT)||(gcHeld&PAD_BUTTON_RIGHT))
                dx+=units;
            if((wpHeld&WPAD_BUTTON_UP)||(gcHeld&PAD_BUTTON_UP))
                dz-=units;
            if((wpHeld&WPAD_BUTTON_DOWN)||(gcHeld&PAD_BUTTON_DOWN))
                dz+=units;
            if(bHeld){
                sceneActor.yaw=(sceneActor.yaw+6u)&1023u;
                dx=0;dz=0;
            }
            if(dx!=0 || dz!=0){
                const bool moved=fitd_scene_move(&sceneActor,
                                                &currentStageRoom,dx,dz);
                actorMovedThisFrame=moved;
                if(moved){
                    const int next=fitd_stage_camera_auto(&originalStage,
                        &currentStageRoom,(int)sceneActor.x,(int)sceneActor.z,
                        (int)originalCamera.current);
                    if(next>=0 && (unsigned)next!=originalCamera.current &&
                       (unsigned)next<originalCamera.frames &&
                       fitd_camera_set(&originalCamera,DATA_DIR,
                                       (unsigned)next,diag)){
                        sceneCamChanges++;
                        stage_refresh_camera();
                        if(diag){
                            fprintf(diag,
                                "FITD-WII-ACTOR06 camera_auto=%d "
                                "pos=%.0f,%.0f yaw=%u\n",
                                next,sceneActor.x,sceneActor.z,
                                sceneActor.yaw);
                            fflush(diag);
                        }
                    }else if(frameCount%12u==0u){
                        stage_refresh_camera();
                    }
                }
            }
        }
        if(actorSceneMode){
            const FitdMotionKind oldMotion=actorMotion.kind;
            fitd_motion_step(&actorMotion,actorMovedThisFrame,
                             passedMs,animationPlaying);
            if(oldMotion!=actorMotion.kind && diag){
                fprintf(diag,
                  "FITD-WII-LOCOMOTION06-3 transition=%s "
                  "clip_source=%s index=%u actor=%.0f,%.0f ticks=%.2f "
                  "changes=%u\n",
                  actorMotion.kind==FITD_MOTION_WALK?"WALK":"IDLE",
                  actorCandidateSlot>=0?"MANUAL_PREVIEW":"REST",
                  actorCandidateSlot>=0?actorCandidateAnimation.index:0u,
                  sceneActor.x,sceneActor.z,actorMotion.ticks,
                  actorMotion.transitions);
                fflush(diag);
            }
        }
        const bool browsingBodies=(showOriginalCamera && originalBody.valid) ||
                                  actorSceneMode;
        bool walking=false;
        // D-pad LEFT/RIGHT selects models while previewing a camera.
        // Do not also translate the actor out of the visible scene.
        if(!browsingBodies && ((wpHeld&WPAD_BUTTON_LEFT)||(gcHeld&PAD_BUTTON_LEFT))){x-=0.045f;walking=true;}
        if(!browsingBodies && ((wpHeld&WPAD_BUTTON_RIGHT)||(gcHeld&PAD_BUTTON_RIGHT))){x+=0.045f;walking=true;}
        if(!browsingBodies && ((wpHeld&WPAD_BUTTON_UP)||(gcHeld&PAD_BUTTON_UP))){z-=0.045f;walking=true;}
        if(!browsingBodies && ((wpHeld&WPAD_BUTTON_DOWN)||(gcHeld&PAD_BUTTON_DOWN))){z+=0.045f;walking=true;}
        if(!actorSceneMode && bHeld)rotation+=1.65f;
        if(x>3.5f)x=3.5f;
        if(x< -3.5f)x=-3.5f;
        if(z>2.f)z=2.f;
        if(z< -3.0f)z=-3.0f;
        phase+=walking?0.15f:0.04f;
        const float walk=walking?sinf(phase):0.f;
        render_gx(x,z,rotation,walk);
        ++frameCount;
        if(diag && frameCount%300==0){
            /* A projected vertex count cannot reveal a deformed skeleton.
               Log local skeletal extents to catch implausible stretching,
               independently of expected distance-based perspective size. */
            if(originalBody.valid){
                float lo[3]={1e9f,1e9f,1e9f};
                float hi[3]={-1e9f,-1e9f,-1e9f};
                for(unsigned vi=0;vi<originalBody.vertex_count;vi++){
                    const float xyz[3]={
                        poseReady?animationPose[vi].x:(float)originalBody.vertices[vi].x,
                        poseReady?animationPose[vi].y:(float)originalBody.vertices[vi].y,
                        poseReady?animationPose[vi].z:(float)originalBody.vertices[vi].z};
                    for(unsigned axis=0;axis<3;axis++){
                        if(xyz[axis]<lo[axis])lo[axis]=xyz[axis];
                        if(xyz[axis]>hi[axis])hi[axis]=xyz[axis];
                    }
                }
                fprintf(diag,
                  "FITD-WII-BONES06-1 actor_mode=%d pose=%d body=%u "
                  "anim=%u group_count=%u root_yaw_suppressed=%d "
                  "extent=%.1f,%.1f,%.1f frame=%u->%u alpha=%.3f\n",
                  actorSceneMode?1:0,poseReady?1:0,originalBody.index,
                  displayedAnim,originalBody.group_count,
                  actorSceneMode?1:0,
                  hi[0]-lo[0],hi[1]-lo[1],hi[2]-lo[2],
                  animationSample.from_frame,animationSample.to_frame,
                  animationSample.fraction);
            }
            FitdSceneScreenPoint center={0};
            const bool projected=fitd_scene_project(&sceneCamera,
                &currentStageRoom,&sceneActor,0.f,0.f,0.f,&center);
            fprintf(diag,
              "FITD-WII-ACTOR06 active=%d x=%.0f y=%.0f z=%.0f "
              "yaw=%u projected=%u origin_screen=%.1f,%.1f "
              "origin_valid=%d active_masks=%u collision_blocks=%u "
              "steps=%u auto_cameras=%u mask_checks=%u\n",
              actorSceneMode?1:0,sceneActor.x,sceneActor.y,sceneActor.z,
              sceneActor.yaw,sceneProjected,center.x,center.y,
              projected?1:0,sceneMaskSelected,sceneActor.collisions,
              sceneActor.steps,sceneCamChanges,sceneMaskChecks);
            fprintf(diag,
              "FITD-WII-LOCOMOTION06-3 state=%s anim=%u "
              "compatible=%d ticks=%.2f transitions=%u "
              "walk_updates=%u active=%d\n",
              actorMotion.kind==FITD_MOTION_WALK?"WALK":"IDLE",
              displayedAnim,
              actorSceneMode?(actorCandidateSlot>=0?1:actorRestReady?1:0):0,
              actorMotion.ticks,actorMotion.transitions,
              actorMotion.walking_updates,actorSceneMode?1:0);
            fprintf(diag,
              "FITD-WII-STAGE05 available=%d floor=%u room=%u "
              "camera=%u mask_preview=%d mask_ready=%d "
              "mask_pixels=%u refreshes=%u selected=%d total_masks=%u\n",
              originalStage.valid?1:0,originalStage.floor_number,
              currentStageRoom.index,originalCamera.current,
              showStageMask?1:0,stageOverlay.ready?1:0,
              stageMasks.covered_pixels,stageMaskRefreshes,
              selectedMaskIndex,stageMasks.masks);
            fprintf(diag,
              "FITD-WII-ANIM04 status=%s compatible=%d playing=%d "
              "entry=%u tick=%.2f pose=%d frame=%u->%u alpha=%.3f\n",
              originalAnimation.valid?"DECODED":"NO_CLIP",
              fitd_anim_can_apply(&originalAnimation,&originalBody)?1:0,
              animationPlaying?1:0,originalAnimation.index,
              animationTicks,poseReady?1:0,
              (unsigned)animationSample.from_frame,
              (unsigned)animationSample.to_frame,animationSample.fraction);
            if(originalBody.valid){
                fprintf(diag,"FITD-WII-MODEL status=visible:%d "
                             "index=%u vertices=%u groups=%u primitives=%u "
                             "rotation=%.1f\n",
                    (showOriginalCamera && showOriginalBody)?1:0,
                    originalBody.index,(unsigned)originalBody.vertex_count,
                    (unsigned)originalBody.group_count,
                    (unsigned)originalBody.primitive_count,rotation);
            }
            const u64 elapsedMs=ticks_to_millisecs(gettime()-startTicks);
            const float fps=elapsedMs?((float)frameCount*1000.f/elapsedMs):0.f;
            fprintf(diag,"GX frames=%lu fps=%.2f actor=%.2f,%.2f yaw=%.1f\n",
                frameCount,fps,x,z,rotation);
            diagnostic_checkpoint();
        }
    }
    if(diag){
        fprintf(diag,"finish frames=%lu endianness_check=%s\n",frameCount,
            fitd_endian_selftest()?"PASS":"FAIL");
        fflush(diag);
        fclose(diag);
        diag=NULL;
    }
    diagnostic_mirror_to_app();
    fitd_mask_gx_release(&stageOverlay);
    fitd_stage_release(&originalStage);
    fitd_camera_release(&originalCamera);
    fitd_body_release(&originalBody);
    
    fitd_anim_release(&actorCandidateAnimation);
    fitd_anim_release(&originalAnimation);
    if(gxFifo)free(gxFifo);
    return 0;
}
