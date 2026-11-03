/*
 * FITD Android ARMv7 — first REAL background frame.
 *
 * This is a scene-preview milestone, not a playable game. The camera image
 * and palette are read by the unmodified open-source FITD PAK parser and
 * decompressor (FitdLib/pak.cpp and unpack.cpp). 320x200 paletted pixels are
 * uploaded into an GLES 2.0 RGBA texture. Preserve 4:3 DOS display aspect.
 *
 * Controller bindings:
 * Left/right D-pad or stick: browse CAMERAxx.PAK scenes (with debounce)
 * L1/R1: browse floors CAMERA00..CAMERA07
 * A: next camera, X: previous camera, B/START: exit
 * Left/right arrows: browse scenes; Page Up/Down: browse floors
 *
 * Original commercial data is NOT embedded in this source / CI build.
 */
#include <SDL.h>
#include <SDL_opengles2.h>
#include <SDL_system.h>
#include "pak.h"
#include "fitd_gamepad_select_sdl2.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <algorithm>
#include <vector>

extern "C" { char homePath[512] = {}; }

namespace {
constexpr int IMAGE_W=320;
constexpr int IMAGE_H=200;
constexpr int IMAGE_SIZE=IMAGE_W*IMAGE_H;
constexpr int PALETTE_SIZE=256*3;
constexpr int FLOOR_LIMIT=8;
constexpr int MAX_ENTRY=128;
SDL_GameController* pad=nullptr;
SDL_JoystickID padId=-1;
int floorNumber=0, cameraNumber=0, numCameras=0;
GLuint texture=0;

bool inAppPrivateData() {
    const char* p=SDL_AndroidGetInternalStoragePath();
    if (!p || strlen(p)+2>=sizeof(homePath)) {
        SDL_Log("FITD-SCENE: E_NO_APP_PRIVATE_DIR");
        return false;
    }
    snprintf(homePath,sizeof(homePath),"%s/",p);
    return true;
}
void closePad() {
    if(pad) { SDL_GameControllerClose(pad); pad=nullptr; padId=-1; }
}
void openPhysicalPad() {
    if(pad) return;
    fitd_android::GamepadChoice choice=fitd_android::selectBestPhysicalGamepad();
    if(choice.index<0) {
        SDL_Log("FITD-SCENE: NO_PHYSICAL_CONTROLLER; keyboard still enabled");
        return;
    }
    if(!SDL_IsGameController(choice.index)) {
        SDL_Log("FITD-SCENE: controller is raw joystick: %s",
                SDL_JoystickNameForIndex(choice.index));
        return; // diagnostic test already has generic fallback
    }
    pad=SDL_GameControllerOpen(choice.index);
    if(pad) {
        padId=SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad));
        SDL_Log("FITD-SCENE: PAD index=%d id=%d name=%s",
                choice.index,(int)padId,SDL_GameControllerName(pad));
    }
}
bool validIndex(const char* base, int index, int exactSize, int ceiling) {
    if(index<0 || index>=MAX_ENTRY) return false;
    // Read fixed-size PAK header ourselves before letting upstream parser
    // operate on a file. Native files are from the app-private installer.
    char fileName[600]={};
    if(snprintf(fileName,sizeof(fileName),"%s%s.PAK",homePath,base)>=(int)sizeof(fileName))
        return false;
    FILE* fp=fopen(fileName,"rb");
    if(!fp) return false;
    bool valid=false;
    if(fseek(fp,0,SEEK_END)==0) {
        long total=ftell(fp);
        if(total>24 && total<=64*1024*1024L && fseek(fp,4,SEEK_SET)==0) {
            unsigned char h[4]={};
            if(fread(h,1,4,fp)==4) {
                uint32_t first=(uint32_t)h[0]|((uint32_t)h[1]<<8)|
                       ((uint32_t)h[2]<<16)|((uint32_t)h[3]<<24);
                if(first>=12 && first<=(uint32_t)total && (first%4)==0 &&
                   index<(int)(first/4)-2) {
                    int n=getPakSize(base,index);
                    valid=(n>=exactSize && n<=ceiling);
                }
            }
        }
    }
    fclose(fp);
    return valid;
}
bool loadPalette(uint8_t out[PALETTE_SIZE]) {
    // AITD1 engine: loadPalette() calls LoadPak("ITD_RESS",3,aux);
    // ITD_RESS #3 is exactly 768 bytes in the original data.
    if(!validIndex("ITD_RESS",3,PALETTE_SIZE,16384)) {
        SDL_Log("FITD-SCENE: E_PALETTE_HEADER");
        return false;
    }
    int n=getPakSize("ITD_RESS",3);
    char* source=loadPak("ITD_RESS",3);
    if(!source) {
        SDL_Log("FITD-SCENE: E_PALETTE_LOAD");
        return false;
    }
    memcpy(out,source,PALETTE_SIZE);
    free(source);
    // Some games use DAC palettes stored as 6 bits/channel; original AITD1
    // palette may contain 0..255 already. Detect without clipping highlights.
    uint8_t maximum=0;
    for(int k=0;k<PALETTE_SIZE;k++)maximum=std::max(maximum,out[k]);
    if(maximum<=63) for(int k=0;k<PALETTE_SIZE;k++)
        out[k]=static_cast<uint8_t>((unsigned(out[k])*255u+31u)/63u);
    SDL_Log("FITD-SCENE: PALETTE ITD_RESS entry=3 bytes=%d max=%u",n,(unsigned)maximum);
    return true;
}
bool renderCamera(int selectedFloor,int selectedCamera) {
    char basename[24]={};
    snprintf(basename,sizeof(basename),"CAMERA%02d",selectedFloor);
    if(!validIndex(basename,selectedCamera,IMAGE_SIZE,IMAGE_SIZE+4096)) {
        SDL_Log("FITD-SCENE: E_CAMERA_HEADER floor=%d camera=%d",
            selectedFloor,selectedCamera);
        return false;
    }
    const int length=getPakSize(basename,selectedCamera);
    // The original engine's loadCamera() reads 64000 paletted pixels.
    char* camera=loadPak(basename,selectedCamera);
    if(!camera) {
        SDL_Log("FITD-SCENE: E_CAMERA_DECOMPRESS %s index=%d",
                basename,selectedCamera);
        return false;
    }
    uint8_t palette[PALETTE_SIZE]={};
    if(!loadPalette(palette)){free(camera);return false;}
    std::vector<uint8_t> rgba(IMAGE_SIZE*4);
    for(int i=0;i<IMAGE_SIZE;i++){
        const uint8_t color=(uint8_t)camera[i];
        rgba[i*4+0]=palette[color*3+0];
        rgba[i*4+1]=palette[color*3+1];
        rgba[i*4+2]=palette[color*3+2];
        rgba[i*4+3]=255;
    }
    free(camera);
    glBindTexture(GL_TEXTURE_2D,texture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,IMAGE_W,IMAGE_H,0,
                 GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
    GLenum error=glGetError();
    if(error!=GL_NO_ERROR){
        SDL_Log("FITD-SCENE: E_TEXTURE_UPLOAD GL=%u",(unsigned)error);
        return false;
    }
    floorNumber=selectedFloor;
    cameraNumber=selectedCamera;
    numCameras=(int)PAK_getNumFiles(basename);
    SDL_Log("FITD-SCENE: FRAME_OK source=%s.PAK index=%d decompressed=%d "
            "size=320x200 rgba=verified available=%d",
            basename,selectedCamera,length,numCameras);
    return true;
}
bool tryFloor(int direction) {
    for(int n=1;n<=FLOOR_LIMIT;n++){
        int next=(floorNumber+direction*n+FLOOR_LIMIT*4)%FLOOR_LIMIT;
        if(renderCamera(next,0))return true;
    }
    return false;
}
bool nextCamera(int direction) {
    if(numCameras<=0)return tryFloor(direction);
    const int base=cameraNumber;
    for(int i=1;i<=numCameras&&i<=MAX_ENTRY;i++) {
        int n=(base+direction*i+numCameras*4)%numCameras;
        if(renderCamera(floorNumber,n))return true;
    }
    return false;
}
GLuint compileShader(GLenum kind,const char* source){
    GLuint sh=glCreateShader(kind);
    glShaderSource(sh,1,&source,nullptr);
    glCompileShader(sh);
    GLint ok=0;glGetShaderiv(sh,GL_COMPILE_STATUS,&ok);
    if(!ok){
        char error[1024]={};
        glGetShaderInfoLog(sh,sizeof(error),nullptr,error);
        SDL_Log("FITD-SCENE: SHADER_ERROR %s",error);
        glDeleteShader(sh);return 0;
    }
    return sh;
}
} // namespace

int main(int argc,char** argv) {
    (void)argc;(void)argv;
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER|SDL_INIT_JOYSTICK)!=0){
        SDL_Log("FITD-SCENE: E_SDL_INIT %s",SDL_GetError());return 1;
    }
    if(!inAppPrivateData()) {SDL_Quit();return 2;}
    SDL_GameControllerEventState(SDL_ENABLE);
    SDL_JoystickEventState(SDL_ENABLE);
    openPhysicalPad();
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    SDL_Window* window=SDL_CreateWindow("Alone in the Dark - first engine scene",
        SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,640,480,
        SDL_WINDOW_OPENGL|SDL_WINDOW_FULLSCREEN);
    if(!window){
        SDL_Log("FITD-SCENE: E_WINDOW %s",SDL_GetError());
        closePad();SDL_Quit();return 3;
    }
    SDL_GLContext gl=SDL_GL_CreateContext(window);
    if(!gl){
        SDL_Log("FITD-SCENE: E_CONTEXT %s",SDL_GetError());
        SDL_DestroyWindow(window);closePad();SDL_Quit();return 4;
    }
    SDL_Log("FITD-SCENE: GL_VERSION %s",
        reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    const char* vs=
        "attribute vec2 pos; attribute vec2 texCoord;"
        "varying vec2 uv; void main(){uv=texCoord;gl_Position=vec4(pos,0.0,1.0);}";
    const char* fs=
        "precision mediump float; varying vec2 uv;uniform sampler2D pic;"
        "void main(){gl_FragColor=texture2D(pic,uv);}";
    GLuint v=compileShader(GL_VERTEX_SHADER,vs),
           f=compileShader(GL_FRAGMENT_SHADER,fs);
    if(!v||!f){
        if(v)glDeleteShader(v);if(f)glDeleteShader(f);
        SDL_GL_DeleteContext(gl);SDL_DestroyWindow(window);closePad();SDL_Quit();
        return 5;
    }
    GLuint program=glCreateProgram();
    glAttachShader(program,v);glAttachShader(program,f);
    glBindAttribLocation(program,0,"pos");
    glBindAttribLocation(program,1,"texCoord");
    glLinkProgram(program);
    GLint linked=0;glGetProgramiv(program,GL_LINK_STATUS,&linked);
    if(!linked){
        SDL_Log("FITD-SCENE: E_LINK");
        glDeleteProgram(program);glDeleteShader(v);glDeleteShader(f);
        SDL_GL_DeleteContext(gl);SDL_DestroyWindow(window);closePad();SDL_Quit();
        return 6;
    }
    // Memory order top row first, GL ES texture UV (0,0) at first row
    // maps correctly to upper-left corner of a screen-space quad.
    const GLfloat quad[]={-1, 1,  0,0,  -1,-1,  0,1,
                            1, 1,  1,0,   1,-1,  1,1};
    GLuint vertexBuffer=0;
    glGenBuffers(1,&vertexBuffer);glBindBuffer(GL_ARRAY_BUFFER,vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER,sizeof(quad),quad,GL_STATIC_DRAW);
    glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);

    bool sceneReady=false;
    for(int fl=0;fl<FLOOR_LIMIT&&!sceneReady;fl++)
        sceneReady=renderCamera(fl,0);
    if(!sceneReady){
        SDL_Log("FITD-SCENE: E_NO_CAMERA; no displayable PAK data; leave safely");
        glDeleteTextures(1,&texture);glDeleteBuffers(1,&vertexBuffer);
        glDeleteProgram(program);glDeleteShader(v);glDeleteShader(f);
        SDL_GL_DeleteContext(gl);SDL_DestroyWindow(window);closePad();SDL_Quit();
        return 7;
    }
    SDL_Log("FITD-SCENE: SUCCESS - first camera frame from actual original PAK");
    SDL_GL_SetSwapInterval(1);
    bool running=true,lastLeft=false,lastRight=false,lastL=false,lastR=false;
    bool lastA=false,lastX=false,lastExit=false;
    Uint32 lastPadScan=0,started=SDL_GetTicks();
    while(running){
        SDL_Event e;
        while(SDL_PollEvent(&e)){
            if(e.type==SDL_QUIT)running=false;
            if(e.type==SDL_KEYDOWN&&!e.key.repeat){
                switch(e.key.keysym.sym){
                    case SDLK_ESCAPE:running=false;break;
                    case SDLK_LEFT:nextCamera(-1);break;
                    case SDLK_RIGHT:nextCamera(1);break;
                    case SDLK_PAGEUP:tryFloor(-1);break;
                    case SDLK_PAGEDOWN:tryFloor(1);break;
                    default:break;
                }
            }
            if(e.type==SDL_CONTROLLERDEVICEREMOVED&&e.cdevice.which==padId){
                SDL_Log("FITD-SCENE: PAD_DISCONNECTED");
                closePad();
            }
        }
        if(!pad && SDL_GetTicks()-lastPadScan>1250) {
            lastPadScan=SDL_GetTicks();
            openPhysicalPad();
        }
        if(pad){
            Sint16 x=SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_LEFTX);
            bool left=x< -14000 ||
                SDL_GameControllerGetButton(pad,SDL_CONTROLLER_BUTTON_DPAD_LEFT);
            bool right=x>14000 ||
                SDL_GameControllerGetButton(pad,SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
            bool shoulderL=SDL_GameControllerGetButton(pad,SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
            bool shoulderR=SDL_GameControllerGetButton(pad,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
            bool a=SDL_GameControllerGetButton(pad,SDL_CONTROLLER_BUTTON_A);
            bool xbutton=SDL_GameControllerGetButton(pad,SDL_CONTROLLER_BUTTON_X);
            bool exitButton=SDL_GameControllerGetButton(pad,SDL_CONTROLLER_BUTTON_B)||
                SDL_GameControllerGetButton(pad,SDL_CONTROLLER_BUTTON_START);
            if(left&&!lastLeft)nextCamera(-1);
            if(right&&!lastRight)nextCamera(1);
            if(shoulderL&&!lastL)tryFloor(-1);
            if(shoulderR&&!lastR)tryFloor(1);
            if(a&&!lastA)nextCamera(1);
            if(xbutton&&!lastX)nextCamera(-1);
            if(exitButton&&!lastExit)running=false;
            lastLeft=left;lastRight=right;lastL=shoulderL;lastR=shoulderR;
            lastA=a;lastX=xbutton;lastExit=exitButton;
        }
        int w=0,h=0;
        SDL_GL_GetDrawableSize(window,&w,&h);
        if(w<1||h<1){SDL_Delay(25);continue;}
        glViewport(0,0,w,h);
        glClearColor(0.0f,0.0f,0.0f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        // Fixed 4:3 display aspect; DOS 320x200 isn't square pixel geometry.
        int sceneW=w,sceneH=(w*3)/4;
        if(sceneH>h){sceneH=h;sceneW=(h*4)/3;}
        glViewport((w-sceneW)/2,(h-sceneH)/2,sceneW,sceneH);
        glUseProgram(program);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D,texture);
        glUniform1i(glGetUniformLocation(program,"pic"),0);
        glBindBuffer(GL_ARRAY_BUFFER,vertexBuffer);
        glEnableVertexAttribArray(0);glEnableVertexAttribArray(1);
        glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(GLfloat),(void*)0);
        glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(GLfloat),(void*)(2*sizeof(GLfloat)));
        glDrawArrays(GL_TRIANGLE_STRIP,0,4);
        SDL_GL_SwapWindow(window);
        SDL_Delay(10);
        // Tester can exit manually; prevent a stuck stage on broken input.
        if(SDL_GetTicks()-started>600000u)running=false;
    }
    glDeleteTextures(1,&texture);glDeleteBuffers(1,&vertexBuffer);
    glDeleteProgram(program);glDeleteShader(v);glDeleteShader(f);
    SDL_GL_DeleteContext(gl);SDL_DestroyWindow(window);
    closePad();SDL_Quit();
    return 0;
}
