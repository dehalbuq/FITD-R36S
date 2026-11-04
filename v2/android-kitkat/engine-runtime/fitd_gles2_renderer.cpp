/*
 * Experimental GLES2 compatibility renderer for the original FITD osystem.h
 * API, used only in the separate Android 4.4 ARMv7 runtime prototype.
 *
 * The original main game loop, animation, script, room and model code remain
 * upstream. This backend converts 320x200 indexed scenes and the legacy
 * polygon primitives into GLES2 calls. No BGFX, ImGui, GLES3 or SDL3.
 *
 * Initial limitations: scene mask compositing, primitive-specific dither,
 * Z-depth occlusion and full UI blending are approximated until validated
 * against the Chanuo A1 hardware. This file is not in the approved Test 07.
 */
#include "common.h"
#include "bgfxGlue.h"
#include <SDL.h>
#include <SDL_opengles2.h>
#include <vector>
#include <map>
#include <utility>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>

SDL_Window* gWindowBGFX=nullptr;
int gFrameLimit=60;
bool gCloseApp=false;
float gVolume=1.f;
int g_screenWidth=320,g_screenHeight=200;
int outputResolution[2]={640,480};
unsigned int gameViewId=0;
char backBuffer[512*256*3]={};
unsigned char frontBuffer[320*200]={};
unsigned char physicalScreen[320*200]={};
std::array<unsigned char,320*200> uiLayer={};

namespace {
static unsigned char palette[256*3]={};
static GLuint texProg=0,polyProg=0,maskProg=0,temporalProg=0,backgroundTexture=0,uiTexture=0;
static std::map<std::pair<int,int>,GLuint> maskTextures;
static unsigned maskRendered=0;
static GLuint quadVbo=0,triVbo=0,cachedQuadVbo=0,cachedFrameTexture=0,historyFrameTexture=0;
static Uint32 lastCaptureTicks=0;
static uint64_t lastBackgroundHash=0;
static bool sameCameraFrame=false;
static bool priorFrameReady=false;
static int cachedWidth=0,cachedHeight=0;
static bool cachedFrameReady=false;
static int targetWidth=640,targetHeight=480,viewportW=640,viewportH=480;
static int viewportX=0,viewportY=0;
struct Vertex{float x,y,z;float r,g,b,a;};
struct QueuedFace{float depth;u8 type;std::vector<Vertex> verts;};
static std::vector<QueuedFace> queuedFaces;
static std::vector<Vertex> queuedLines;
static std::array<uint8_t,320*200*4> converted{};
static bool graphicsReady=false;
static bool clipEnabled=false;
static float clipCoords[4]={0,0,320,200};

static void rgbaFromIndexed(const unsigned char* data,bool transparent) {
    for(int k=0;k<320*200;k++){
        const unsigned char index=data[k];
        converted[k*4+0]=palette[index*3+0];
        converted[k*4+1]=palette[index*3+1];
        converted[k*4+2]=palette[index*3+2];
        converted[k*4+3]=(transparent && index==0)?0:255;
    }
}
static GLuint compileShader(GLenum kind,const char* text) {
    GLuint s=glCreateShader(kind);
    glShaderSource(s,1,&text,nullptr);glCompileShader(s);
    GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok) {
        char err[512]={};
        glGetShaderInfoLog(s,sizeof(err),nullptr,err);
        SDL_Log("FITD-GLES2: shader compile failed: %s",err);
        glDeleteShader(s);return 0;
    }
    return s;
}
static GLuint makeProgram(const char*vs,const char*fs,
                          const char* a0,const char* a1) {
    GLuint v=compileShader(GL_VERTEX_SHADER,vs),
           f=compileShader(GL_FRAGMENT_SHADER,fs);
    if(!v||!f)return 0;
    GLuint prog=glCreateProgram();
    glAttachShader(prog,v);glAttachShader(prog,f);
    glBindAttribLocation(prog,0,a0);glBindAttribLocation(prog,1,a1);
    glLinkProgram(prog);
    GLint ok=0;glGetProgramiv(prog,GL_LINK_STATUS,&ok);
    glDeleteShader(v);glDeleteShader(f);
    if(!ok) {
        SDL_Log("FITD-GLES2: program link failed");
        glDeleteProgram(prog);return 0;
    }
    return prog;
}
static void updateViewport(){
    int dw=640,dh=480;
    if(gWindowBGFX)SDL_GL_GetDrawableSize(gWindowBGFX,&dw,&dh);
    if(dw<1||dh<1)return;
    targetWidth=dw;targetHeight=dh;
    viewportW=dw;
    viewportH=(dw*3)/4;
    if(viewportH>dh){viewportH=dh;viewportW=(dh*4)/3;}
    viewportX=(dw-viewportW)/2;
    viewportY=(dh-viewportH)/2;
    outputResolution[0]=dw;outputResolution[1]=dh;
    glViewport(viewportX,viewportY,viewportW,viewportH);
}
static void drawTexture(GLuint tex,const unsigned char* data,bool useAlpha) {
    if(!graphicsReady||!tex)return;
    rgbaFromIndexed(data,useAlpha);
    glBindTexture(GL_TEXTURE_2D,tex);
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,320,200,GL_RGBA,
                    GL_UNSIGNED_BYTE,converted.data());
    // Both the photographic background and the indexed UI are independent
    // of the actor depth buffer. Never let a full-screen quad occlude a model.
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    if(useAlpha) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    } else glDisable(GL_BLEND);
    glUseProgram(texProg);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,tex);
    glUniform1i(glGetUniformLocation(texProg,"pic"),0);
    glBindBuffer(GL_ARRAY_BUFFER,quadVbo);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(GLfloat),(void*)0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(GLfloat),
                          (void*)(2*sizeof(GLfloat)));
    glDrawArrays(GL_TRIANGLE_STRIP,0,4);
    glDisableVertexAttribArray(0);glDisableVertexAttribArray(1);
    glDisable(GL_BLEND);
}
static void drawVertices(GLenum primitive,const std::vector<Vertex>& data) {
    if(!graphicsReady||data.empty())return;
    std::vector<float> f;
    f.reserve(data.size()*7);
    for(const Vertex& v:data) {
        f.push_back(v.x/160.f-1.f);
        f.push_back(1.f-v.y/100.f);
        // Preserve original 3D Z ordering instead of painter-sorting entire
        // polygons by their mean depth (which broke intersecting body parts).
        f.push_back(v.z/(std::fabs(v.z)+1024.f));
        f.push_back(v.r);f.push_back(v.g);
        f.push_back(v.b);f.push_back(v.a);
    }
    glUseProgram(polyProg);
    glBindBuffer(GL_ARRAY_BUFFER,triVbo);
    glBufferData(GL_ARRAY_BUFFER,f.size()*sizeof(float),f.data(),GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);glEnableVertexAttribArray(1);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,7*sizeof(float),(void*)0);
    glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,7*sizeof(float),
                          (void*)(3*sizeof(float)));
    glDrawArrays(primitive,0,(GLsizei)data.size());
    glDisableVertexAttribArray(0);glDisableVertexAttribArray(1);
}
static Vertex vtx(float x,float y,int index,float z=0.f,float alpha=1.f) {
    return {x,y,z,
        palette[(index&255)*3+0]/255.f,
        palette[(index&255)*3+1]/255.f,
        palette[(index&255)*3+2]/255.f,alpha};
}
static bool initGL(){
    if(graphicsReady)return true;
    const char* tV="attribute vec2 p;attribute vec2 uv;"
                   "varying vec2 t;void main(){t=uv;gl_Position=vec4(p,0.,1.);}";
    const char* tF="precision mediump float;varying vec2 t;"
                   "uniform sampler2D pic;void main(){gl_FragColor=texture2D(pic,t);}";
    const char* pV="attribute vec3 p;attribute vec4 color;"
                   "varying vec4 col;void main(){col=color;gl_Position=vec4(p,1.);}";
    const char* pF="precision mediump float;varying vec4 col;"
                   "void main(){gl_FragColor=col;}";
    const char* maskF=
        "precision mediump float;varying vec2 t;"
        "uniform sampler2D maskTex;uniform sampler2D bgTex;"
        "void main(){float visible=texture2D(maskTex,t).r;"
        "if(visible<0.001)discard;"
        "gl_FragColor=texture2D(bgTex,t);}";
    // GPU temporal blend: present 60 visually distinct updates per second,
    // while original AI, collision, scripts and saves keep 40ms logic ticks.
    // Delay presentation by one game tick so alpha moves forward (no rewind).
    const char* temporalF=
        "precision mediump float;varying vec2 t;"
        "uniform sampler2D oldFrame;uniform sampler2D newFrame;"
        "uniform float blendAmount;"
        "void main(){gl_FragColor=mix(texture2D(oldFrame,t),"
        "texture2D(newFrame,t),blendAmount);}";
    texProg=makeProgram(tV,tF,"p","uv");
    polyProg=makeProgram(pV,pF,"p","color");
    maskProg=makeProgram(tV,maskF,"p","uv");
    temporalProg=makeProgram(tV,temporalF,"p","uv");
    if(!texProg||!polyProg||!maskProg||!temporalProg)return false;
    static const GLfloat quad[]={-1,1,0,0, -1,-1,0,1,
                                  1,1,1,0, 1,-1,1,1};
    glGenBuffers(1,&quadVbo);
    glBindBuffer(GL_ARRAY_BUFFER,quadVbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(quad),quad,GL_STATIC_DRAW);
    glGenBuffers(1,&triVbo);
    // GPU-side backbuffer cache for up to 60 presentations/s while the
    // original AI, collision and scripts continue at fixed 25 Hz.
    static const GLfloat screenQuad[]={
        -1.f, 1.f,0.f,1.f, -1.f,-1.f,0.f,0.f,
         1.f, 1.f,1.f,1.f,  1.f,-1.f,1.f,0.f};
    glGenBuffers(1,&cachedQuadVbo);
    glBindBuffer(GL_ARRAY_BUFFER,cachedQuadVbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(screenQuad),screenQuad,GL_STATIC_DRAW);
    glGenTextures(1,&cachedFrameTexture);
    glGenTextures(1,&historyFrameTexture);
    for(GLuint tex:{cachedFrameTexture,historyFrameTexture}) {
        glBindTexture(GL_TEXTURE_2D,tex);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    }
    glGenTextures(1,&backgroundTexture);
    glGenTextures(1,&uiTexture);
    for(GLuint tex:{backgroundTexture,uiTexture}){
        glBindTexture(GL_TEXTURE_2D,tex);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,320,200,0,GL_RGBA,
                      GL_UNSIGNED_BYTE,nullptr);
    }
    graphicsReady=true;
    SDL_Log("FITD-GLES2: initialised real engine osystem backend");
    return true;
}
} // namespace

int initBgfxGlue(int,char**) {
    // Retain upstream function signature; no BGFX object is created.
    const char* v=reinterpret_cast<const char*>(glGetString(GL_VERSION));
    SDL_Log("FITD-GLES2: original game renderer using GLES2, version=%s",
            v?v:"unknown");
    return initGL()?1:0;
}
void deleteBgfxGlue() {
    if(quadVbo)glDeleteBuffers(1,&quadVbo);
    if(triVbo)glDeleteBuffers(1,&triVbo);
    if(cachedQuadVbo)glDeleteBuffers(1,&cachedQuadVbo);
    if(cachedFrameTexture)glDeleteTextures(1,&cachedFrameTexture);
    if(historyFrameTexture)glDeleteTextures(1,&historyFrameTexture);
    cachedFrameReady=false; cachedWidth=0; cachedHeight=0;
    if(backgroundTexture)glDeleteTextures(1,&backgroundTexture);
    if(uiTexture)glDeleteTextures(1,&uiTexture);
    if(texProg)glDeleteProgram(texProg);
    if(polyProg)glDeleteProgram(polyProg);
    if(maskProg)glDeleteProgram(maskProg);
    if(temporalProg)glDeleteProgram(temporalProg);
    for(const auto& item:maskTextures){
        GLuint tex=item.second;
        glDeleteTextures(1,&tex);
    }
    maskTextures.clear();
    maskProg=0;texProg=0;polyProg=0;temporalProg=0;
    graphicsReady=false;priorFrameReady=false;cachedFrameReady=false;
}
void StartFrame() {
    if(!graphicsReady)return;
    updateViewport();
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.f,0.f,0.f,1.f);
    glClearDepthf(1.f);
    glViewport(0,0,targetWidth,targetHeight);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    updateViewport();
}
void EndFrame() {
    if(gWindowBGFX)SDL_GL_SwapWindow(gWindowBGFX);
}
// The GLES context belongs exclusively to the engine worker. The UI
// thread never touches these functions, avoiding EGL ownership races.
static uint64_t backgroundFingerprint() {
    uint64_t hash=1469598103934665603ULL;
    for(unsigned char pixel:physicalScreen){
        hash^=pixel;
        hash*=1099511628211ULL;
    }
    return hash;
}
extern "C" void fitdAndroidCaptureFrame() {
    if(!graphicsReady || !cachedFrameTexture || !historyFrameTexture ||
       targetWidth<1 || targetHeight<1)return;
    const bool resized=(cachedWidth!=targetWidth || cachedHeight!=targetHeight);
    if(resized) {
        cachedFrameReady=false;priorFrameReady=false;
        cachedWidth=targetWidth;cachedHeight=targetHeight;
        for(GLuint tex:{cachedFrameTexture,historyFrameTexture}){
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D,tex);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,cachedWidth,cachedHeight,0,
                         GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        }
    }
    // Preserve previous completed frame entirely on GPU; no CPU readback.
    if(cachedFrameReady)std::swap(cachedFrameTexture,historyFrameTexture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,cachedFrameTexture);
    glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,0,0,cachedWidth,cachedHeight);
    const bool ok=(glGetError()==GL_NO_ERROR);
    priorFrameReady=cachedFrameReady && ok;
    cachedFrameReady=ok;
    const uint64_t fingerprint=backgroundFingerprint();
    // Only blend frames within the same photographic background; a camera
    // cut, palette reset or menu switch must remain crisp, never ghosted.
    sameCameraFrame=priorFrameReady && lastBackgroundHash==fingerprint;
    lastBackgroundHash=fingerprint;
    lastCaptureTicks=SDL_GetTicks();
}
extern "C" bool fitdAndroidPresentCachedFrame() {
    if(!graphicsReady || !cachedFrameReady || !gWindowBGFX)return false;
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);
    glViewport(0,0,cachedWidth,cachedHeight);
    const bool tween=sameCameraFrame && priorFrameReady;
    glUseProgram(tween?temporalProg:texProg);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,tween?historyFrameTexture:cachedFrameTexture);
    glUniform1i(glGetUniformLocation(tween?temporalProg:texProg,
                                    tween?"oldFrame":"pic"),0);
    if(tween){
        // Between genuine 25Hz render samples, output up to 60Hz unique
        // blended images; this is VISUAL interpolation, not gameplay ticks.
        const float phase=std::max(0.f,std::min(1.f,
                             (SDL_GetTicks()-lastCaptureTicks)/40.f));
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D,cachedFrameTexture);
        glUniform1i(glGetUniformLocation(temporalProg,"newFrame"),1);
        glUniform1f(glGetUniformLocation(temporalProg,"blendAmount"),phase);
    }
    glBindBuffer(GL_ARRAY_BUFFER,cachedQuadVbo);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(GLfloat),(void*)0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(GLfloat),
                          (void*)(2*sizeof(GLfloat)));
    glDrawArrays(GL_TRIANGLE_STRIP,0,4);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glActiveTexture(GL_TEXTURE0);
    SDL_GL_SwapWindow(gWindowBGFX);
    return true;
}
void renderGameWindow(){}
void osystem_preinigGL(){}
void osystem_initGL(int width,int height) {
    g_screenWidth=width;g_screenHeight=height;
}
void osystem_initBuffer() {
    memset(frontBuffer,0,sizeof(frontBuffer));
    memset(physicalScreen,0,sizeof(physicalScreen));
}
void osystem_setPalette(u8* p) {
    if(p)memcpy(palette,p,sizeof(palette));
}
void osystem_setPalette(palette_t* p) {
    if(!p)return;
    for(int k=0;k<256;k++)
        for(int c=0;c<3;c++)palette[k*3+c]=(*p)[k][c];
}
void osystem_setPalette320x200(byte* p) {
    osystem_setPalette(reinterpret_cast<u8*>(p));
}
void osystem_setColor(byte index,byte r,byte g,byte b) {
    int i=(uint8_t)index;
    palette[i*3]=r;palette[i*3+1]=g;palette[i*3+2]=b;
}
void osystem_CopyBlockPhys(unsigned char* buffer,int l,int t,int r,int b) {
    if(!buffer)return;
    l=std::max(0,std::min(320,l));
    r=std::max(l,std::min(320,r));
    t=std::max(0,std::min(200,t));
    b=std::max(t,std::min(200,b));
    for(int row=t;row<b;row++)
        memcpy(physicalScreen+row*320+l,buffer+row*320+l,r-l);
}
void osystem_draw320x200BufferToScreen(unsigned char* buffer) {
    if(buffer) {
        memcpy(physicalScreen,buffer,320*200);
        osystem_drawBackground();
    }
}
void osystem_initVideoBuffer(char* buffer,int w,int h){
    (void)buffer;(void)w;(void)h;
}
void osystem_putpixel(int x,int y,int index){
    if(x>=0&&x<320&&y>=0&&y<200)
        physicalScreen[y*320+x]=(uint8_t)index;
}
void osystem_refreshFrontTextureBuffer(){}
void osystem_drawBackground() {drawTexture(backgroundTexture,physicalScreen,false);}
void osystem_drawUILayer() {drawTexture(uiTexture,uiLayer.data(),true);}
void osystem_startFrame() {
    if(!graphicsReady && !initGL())return;
    queuedFaces.clear();
    queuedLines.clear();
    osystem_drawBackground();
}
void osystem_stopFrame(){}
void osystem_cleanScreenKeepZBuffer(){
    // Background redraw restores a clean image for the original actor pass.
    osystem_drawBackground();
}
void osystem_setClip(float left,float top,float right,float bottom){
    clipEnabled=true;
    clipCoords[0]=left;clipCoords[1]=top;
    clipCoords[2]=right;clipCoords[3]=bottom;
    const int x=std::max(0,(int)(left*viewportW/320.f));
    const int y=std::max(0,(int)((200.f-bottom)*viewportH/200.f));
    const int w=std::max(0,(int)((right-left)*viewportW/320.f));
    const int h=std::max(0,(int)((bottom-top)*viewportH/200.f));
    glEnable(GL_SCISSOR_TEST);
    glScissor(viewportX+x,viewportY+y,w,h);
}
void osystem_clearClip(){clipEnabled=false;glDisable(GL_SCISSOR_TEST);}
void osystem_fillPoly(float* points,int count,unsigned char color,u8 type){
    if(!points||count<3||count>80)return;
    QueuedFace face;
    face.depth=0.f; face.type=type;
    float minX=points[0],maxX=points[0],minY=points[1];
    for(int n=0;n<count;n++){
        face.depth+=points[3*n+2];
        minX=std::min(minX,points[3*n]);
        maxX=std::max(maxX,points[3*n]);
        minY=std::min(minY,points[3*n+1]);
    }
    face.depth/=count;
    const float width=std::max(1.f,maxX-minX);
    const int bank=color&0xf0;
    const int start=color&0xf;
    face.verts.reserve((count-2)*3);
    for(int n=1;n<count-1;n++){
        for(int idx:{0,n,n+1}){
            const float x=points[3*idx],y=points[3*idx+1],z=points[3*idx+2];
            int shade=start;
            if(type==3)shade+=int(15.f*(x-minX)/width);
            else if(type==6)shade+=int(15.f*(maxX-x)/width);
            else if(type==4)shade+=int(y-minY);
            else if(type==5)shade+=int((y-minY)*0.5f);
            shade=std::max(0,std::min(15,shade));
            face.verts.push_back(vtx(x,y,bank|shade,z,type==2?0.55f:1.f));
        }
    }
    queuedFaces.push_back(std::move(face));
}
void osystem_draw3dLine(float x1,float y1,float z1,float x2,float y2,float z2,
                         unsigned char color){
    queuedLines.push_back(vtx(x1,y1,color,z1));
    queuedLines.push_back(vtx(x2,y2,color,z2));
}
void osystem_draw3dQuad(float x1,float y1,float z1,float x2,float y2,float z2,
                        float x3,float y3,float z3,float x4,float y4,float z4,
                        unsigned char color,int transparency){
    (void)transparency;
    osystem_draw3dLine(x1,y1,z1,x2,y2,z2,color);
    osystem_draw3dLine(x2,y2,z2,x3,y3,z3,color);
    osystem_draw3dLine(x3,y3,z3,x4,y4,z4,color);
    osystem_draw3dLine(x4,y4,z4,x1,y1,z1,color);
}
void osystem_drawPoint(float x,float y,float z,u8 color,u8,float size){
    // Original sphere/point impostor uses size*6/5. A depth-preserving
    // triangle fan avoids the Test10 axis-aligned square artifact.
    if(size<=0.f)return;
    const float radius=std::max(.5f,std::min(180.f,size*1.2f));
    QueuedFace face;
    face.depth=z;face.type=0;
    const float pi=3.14159265358979323846f;
    for(int i=0;i<12;++i){
        const float a=2.f*pi*i/12.f;
        const float b=2.f*pi*(i+1)/12.f;
        face.verts.push_back(vtx(x,y,color,z));
        face.verts.push_back(vtx(x+std::cos(a)*radius,
                                 y+std::sin(a)*radius,color,z));
        face.verts.push_back(vtx(x+std::cos(b)*radius,
                                 y+std::sin(b)*radius,color,z));
    }
    queuedFaces.push_back(std::move(face));
}
void osystem_drawSphere(float x,float y,float z,u8 color,u8 material,float size){
    osystem_drawPoint(x,y,z,color,material,size);
}
void osystem_flushPendingPrimitives(){
    if(queuedFaces.empty()&&queuedLines.empty())return;
    std::vector<Vertex> opaque,translucent;
    for(const QueuedFace& face:queuedFaces){
        auto& dst=(face.type==2)?translucent:opaque;
        dst.insert(dst.end(),face.verts.begin(),face.verts.end());
    }
    // Original renderer uses a per-pixel Z buffer, not sorting whole actors
    // by mean Z. Restore the former to fix intersecting/overlapping meshes.
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    drawVertices(GL_TRIANGLES,opaque);
    glDepthMask(GL_FALSE);
    if(!translucent.empty()){
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        drawVertices(GL_TRIANGLES,translucent);
        glDisable(GL_BLEND);
    }
    drawVertices(GL_LINES,queuedLines);
    glDisable(GL_DEPTH_TEST);
    queuedFaces.clear();queuedLines.clear();
}
void osystem_flip(unsigned char* buffer){
    if(buffer)osystem_draw320x200BufferToScreen(buffer);
    osystem_flushPendingPrimitives();
}
void osystem_createMask(const std::array<u8,320*200>& mask,
                        int roomId,int maskId,int,int,int,int){
    if(!graphicsReady||roomId<0||roomId>255||maskId<0||maskId>255)return;
    auto id=std::make_pair(roomId,maskId);
    auto existing=maskTextures.find(id);
    if(existing!=maskTextures.end()){
        GLuint oldTex=existing->second;
        glDeleteTextures(1,&oldTex);
        maskTextures.erase(existing);
    }
    if(maskTextures.size()>=512){
        SDL_Log("FITD-GLES2: mask texture limit reached");
        return;
    }
    GLuint tex=0;glGenTextures(1,&tex);
    if(!tex)return;
    glBindTexture(GL_TEXTURE_2D,tex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    // Original camera mask is 320x200 indexed bytes; each nonzero byte
    // restores the original background OVER an already drawn 3D actor.
    // GL_LUMINANCE is supported by GLES2 without integer textures.
    glTexImage2D(GL_TEXTURE_2D,0,GL_LUMINANCE,320,200,0,
                 GL_LUMINANCE,GL_UNSIGNED_BYTE,mask.data());
    maskTextures[id]=tex;
    SDL_Log("FITD-GLES2: mask created room=%d mask=%d total=%u",
        roomId,maskId,(unsigned)maskTextures.size());
}
void osystem_drawMask(int roomId,int maskId){
    if(!graphicsReady||!maskProg)return;
    // Apply the camera's occlusion over actors already drawn this frame.
    // If geometry is still queued, a mask drawn first cannot occlude it.
    osystem_flushPendingPrimitives();
    auto it=maskTextures.find(std::make_pair(roomId,maskId));
    if(it==maskTextures.end())return;
    glDisable(GL_BLEND);
    glUseProgram(maskProg);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,it->second);
    glUniform1i(glGetUniformLocation(maskProg,"maskTex"),0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D,backgroundTexture);
    glUniform1i(glGetUniformLocation(maskProg,"bgTex"),1);
    glBindBuffer(GL_ARRAY_BUFFER,quadVbo);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(float),
                          (void*)(2*sizeof(float)));
    glDrawArrays(GL_TRIANGLE_STRIP,0,4);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glActiveTexture(GL_TEXTURE0);
    ++maskRendered;
    if(maskRendered%500==1)
        SDL_Log("FITD-GLES2: occlusion mask drawn count=%u floor=%d mask=%d",
                maskRendered,roomId,maskId);
}
void osystem_startBgPoly(){}
void osystem_endBgPoly(){}
void osystem_addBgPolyPoint(int,int){}
void osystem_drawLine(int x1,int x2,int y1,int y2,unsigned char color,
                      unsigned char*){
    osystem_draw3dLine(x1,y1,0,x2,y2,0,color);
}
