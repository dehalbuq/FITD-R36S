#include "fitd_character_preview.h"
#include "pak.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <vector>

namespace fitd_character {
namespace {
GLuint compile(GLenum type,const char* src){
    GLuint s=glCreateShader(type);
    glShaderSource(s,1,&src,nullptr);
    glCompileShader(s);
    GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok){
        char error[1024]={};
        glGetShaderInfoLog(s,sizeof(error),nullptr,error);
        SDL_Log("FITD-MODEL: GLES2 SHADER_FAIL %s",error);
        glDeleteShader(s);return 0;
    }
    return s;
}
struct Projected {float x,y,depth;};
struct Surface {float depth;std::vector<float> points;};
}
bool ModelPreview::init(){
    const char* vsSrc=
        "attribute vec2 p; attribute vec3 c; varying vec3 color;"
        "void main(){color=c;gl_Position=vec4(p,0.0,1.0);}";
    const char* fsSrc=
        "precision mediump float; varying vec3 color;"
        "void main(){gl_FragColor=vec4(color,1.0);}";
    vs=compile(GL_VERTEX_SHADER,vsSrc);fs=compile(GL_FRAGMENT_SHADER,fsSrc);
    if(!vs||!fs){shutdown();return false;}
    shader=glCreateProgram();
    glAttachShader(shader,vs);glAttachShader(shader,fs);
    glBindAttribLocation(shader,0,"p");
    glBindAttribLocation(shader,1,"c");
    glLinkProgram(shader);
    GLint linked=0;glGetProgramiv(shader,GL_LINK_STATUS,&linked);
    if(!linked){SDL_Log("FITD-MODEL: LINK_FAIL");shutdown();return false;}
    glGenBuffers(1,&vbo);
    if(!vbo){shutdown();return false;}
    const int palBytes=getPakSize("ITD_RESS",3);
    char* bytes=(palBytes>=768&&palBytes<=16384)?loadPak("ITD_RESS",3):nullptr;
    if(bytes){
        uint8_t maxi=0;
        for(int k=0;k<768;k++)maxi=std::max(maxi,(uint8_t)bytes[k]);
        const float factor=maxi<=63?1.f/63.f:1.f/255.f;
        for(int k=0;k<768;k++)palette[k]=(uint8_t)bytes[k]*factor;
        free(bytes);
    }else{
        // Fallback is only for the optional preview mesh, not gameplay.
        for(int k=0;k<256;k++)
            for(int c=0;c<3;c++)palette[k*3+c]=(float)k/255.f;
    }
    return select(0,1);
}
bool ModelPreview::loadFrom(const char* base,int index,fitd_body::Mesh& out){
    if(index<0||index>270)return false;
    const unsigned count=PAK_getNumFiles(base);
    if(index>=(int)count)return false;
    const int length=getPakSize(base,index);
    if(length<32||length>131072)return false;
    // Native original FITD loader and decompressor (unmodified).
    char* ptr=loadPak(base,index);
    if(!ptr)return false;
    const bool ok=fitd_body::parse((const uint8_t*)ptr,
                                  (size_t)length,out);
    free(ptr);
    return ok;
}
bool ModelPreview::tryIndex(int index){
    fitd_body::Mesh candidate;
    if(!loadFrom("LISTBODY",index,candidate) &&
       !loadFrom("LISTBOD2",index,candidate))return false;
    // Prefer a multi-polygon 3D mesh over trivial sprites/lines.
    int faces=0;
    for(const auto& p:candidate.primitives)
        if(p.vertices.size()>=3)faces++;
    if(faces<4)return false;
    selectedIndex=index;
    mesh=std::move(candidate);
    SDL_Log("FITD-MODEL: BODY_LOADED index=%d vertices=%d faces=%d "
        "bounds=(%.0f,%.0f,%.0f)-(%.0f,%.0f,%.0f)",
        selectedIndex,(int)mesh.vertices.size(),faces,
        mesh.minX,mesh.minY,mesh.minZ,
        mesh.maxX,mesh.maxY,mesh.maxZ);
    return true;
}
bool ModelPreview::select(int startIndex,int direction){
    const int sign=direction<0?-1:1;
    for(int n=0;n<90;n++){
        const int index=(startIndex+sign*n+271*90)%271;
        if(tryIndex(index))return true;
    }
    SDL_Log("FITD-MODEL: NO_VALID_BODY after model scan from %d",startIndex);
    return false;
}
void ModelPreview::move(uint8_t mask){
    const float step=0.010f;
    if(mask&1)actorY=std::min(0.45f,actorY+step);
    if(mask&2)actorY=std::max(-0.60f,actorY-step);
    if(mask&4)actorX=std::max(-0.70f,actorX-step);
    if(mask&8)actorX=std::min(0.70f,actorX+step);
}
void ModelPreview::turn(float degrees){
    rotation+=degrees*(3.14159265f/180.f);
}
void ModelPreview::draw(){
    if(!shader||!vbo||mesh.vertices.empty())return;
    const float centerX=(mesh.minX+mesh.maxX)*0.5f;
    const float centerY=(mesh.minY+mesh.maxY)*0.5f;
    const float centerZ=(mesh.minZ+mesh.maxZ)*0.5f;
    const float sx=mesh.maxX-mesh.minX,sy=mesh.maxY-mesh.minY,sz=mesh.maxZ-mesh.minZ;
    const float extent=std::max(10.f,std::max(sx,std::max(sy,sz)));
    const float scale=1.4f/extent;
    const float co=cosf(rotation),si=sinf(rotation);
    std::vector<Projected> projected;
    projected.reserve(mesh.vertices.size());
    for(const auto& v:mesh.vertices){
        float x=(v.x-centerX)*scale,y=(v.y-centerY)*scale,z=(v.z-centerZ)*scale;
        const float rotX=x*co+z*si;
        const float rotZ=z*co-x*si;
        const float perspective=1.65f/(2.9f+rotZ);
        projected.push_back({actorX+rotX*perspective*0.9f,
                             actorY+y*perspective*0.9f,rotZ});
    }
    std::vector<Surface> surfaces;
    surfaces.reserve(mesh.primitives.size());
    for(const auto& p:mesh.primitives){
        if(p.vertices.size()<3)continue;
        Surface face;
        face.depth=0.f;
        for(uint16_t i:p.vertices)face.depth+=projected[i].depth;
        face.depth/=p.vertices.size();
        // Primitive materials and palette indices come from actual FITD body.
        // This pass uses flat palette colors; lighting, masks and Z-depth
        // against room geometry remain for the full render backend.
        const float r=palette[p.color*3],g=palette[p.color*3+1],b=palette[p.color*3+2];
        for(size_t k=1;k+1<p.vertices.size();++k){
            for(uint16_t index:{p.vertices[0],p.vertices[k],p.vertices[k+1]}){
                const Projected& vertex=projected[index];
                face.points.insert(face.points.end(),
                    {vertex.x,vertex.y,r,g,b});
            }
        }
        if(!face.points.empty())surfaces.push_back(std::move(face));
    }
    std::sort(surfaces.begin(),surfaces.end(),
        [](const Surface& a,const Surface& b){return a.depth>b.depth;});
    std::vector<float> payload;
    size_t count=0;
    for(const auto& face:surfaces)count+=face.points.size();
    payload.reserve(count);
    for(const auto& face:surfaces)
        payload.insert(payload.end(),face.points.begin(),face.points.end());
    if(payload.empty())return;
    glDisable(GL_BLEND);
    glUseProgram(shader);
    glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,payload.size()*sizeof(float),
                 payload.data(),GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);glEnableVertexAttribArray(1);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,5*sizeof(float),
                          (void*)(2*sizeof(float)));
    glDrawArrays(GL_TRIANGLES,0,(GLsizei)(payload.size()/5));
    SDL_LogVerbose(SDL_LOG_CATEGORY_APPLICATION,
        "FITD-MODEL: draw body=%d vertices=%d",selectedIndex,
        (int)(payload.size()/5));
    glDisableVertexAttribArray(0);glDisableVertexAttribArray(1);
}
void ModelPreview::shutdown(){
    mesh=fitd_body::Mesh{};
    if(vbo){glDeleteBuffers(1,&vbo);vbo=0;}
    if(shader){glDeleteProgram(shader);shader=0;}
    if(vs){glDeleteShader(vs);vs=0;}
    if(fs){glDeleteShader(fs);fs=0;}
}
} // namespace fitd_character
