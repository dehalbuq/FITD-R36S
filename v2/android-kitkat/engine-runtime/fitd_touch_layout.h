// Shared geometry between SDL2 touch input and the OpenGL ES 2.0 overlay.
// Screen coordinates are normalized and start at the TOP-LEFT.
// Keep the actual gameplay in a centered 4:3 viewport without distortion.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace fitd_touch {
enum ButtonId { ACTION, BACK, CONFIRM, MENU };
struct Button { ButtonId id; float x,y,r; const char* label; };
struct Stick { float x=0.f,y=0.f,r=0.f; };
struct Layout {
    bool active=false;
    bool portrait=false;
    int x=0,y=0,w=0,h=0; // GL drawable pixels; bottom-left origin
    Stick stick{};
    std::array<Button,4> buttons{};
};
inline Layout compute(int width,int height,bool showTouch){
    Layout out;
    if(width<=0||height<=0)return out;
    out.active=showTouch;
    out.portrait=height>width;
    float availableW=(float)width;
    float availableH=(float)height;
    if(showTouch){
        if(out.portrait){
            availableW=width*.97f;
            availableH=height*.56f;
        }else{
            availableW=width*.52f;
            availableH=height*.94f;
        }
    }
    const float gameW=std::min(availableW,availableH*4.f/3.f);
    const float gameH=gameW*.75f;
    out.w=std::max(1,(int)std::floor(gameW));
    out.h=std::max(1,(int)std::floor(gameH));
    out.x=(width-out.w)/2;
    // With touch: center game in the upper play area in portrait, centered
    // horizontally between the controls in landscape. Without touch: center
    // against the FULL device display, including when in portrait orientation.
    const float top=(availableH-gameH)*.5f;
    out.y=height-(int)std::floor(top)-out.h;
    if(!showTouch)return out;
    if(out.portrait){
        out.stick={.235f,.765f,.165f};
        const float radius=.068f;
        out.buttons={{
            {ACTION,.78f,.827f,radius,"A"},
            {BACK,.90f,.727f,radius,"B"},
            {CONFIRM,.76f,.632f,radius,"Y"},
            {MENU,.60f,.738f,radius,"M"},
        }};
    }else{
        out.stick={.128f,.62f,.166f};
        const float radius=.069f;
        const float shift=.095f*(float)height/width;
        out.buttons={{
            {ACTION,.864f,.760f,radius,"A"},
            {BACK,.864f+shift,.590f,radius,"B"},
            {CONFIRM,.864f,.425f,radius,"Y"},
            {MENU,.864f-shift,.590f,radius,"M"},
        }};
    }
    return out;
}
inline float clamp1(float v){return std::max(-1.f,std::min(1.f,v));}
inline bool isStickStart(int width,int height,float nx,float ny){
    if(width<=0||height<=0)return false;
    const auto layout=compute(width,height,true);
    const float size=(float)std::min(width,height);
    const float dx=(nx-layout.stick.x)*width/size;
    const float dy=(ny-layout.stick.y)*height/size;
    return dx*dx+dy*dy<=layout.stick.r*layout.stick.r*1.35f*1.35f;
}
inline void stickAxes(int width,int height,float nx,float ny,float& x,float& y){
    x=y=0.f;
    if(width<=0||height<=0)return;
    const auto layout=compute(width,height,true);
    const float size=(float)std::min(width,height);
    const float travel=layout.stick.r*size*.72f;
    if(travel<=0.f)return;
    x=clamp1((nx-layout.stick.x)*width/travel);
    y=clamp1((ny-layout.stick.y)*height/travel);
    const float length=std::sqrt(x*x+y*y);
    if(length>1.f){x/=length;y/=length;}
}
inline int buttonHit(int width,int height,float nx,float ny){
    if(width<=0||height<=0)return -1;
    const auto layout=compute(width,height,true);
    const float size=(float)std::min(width,height);
    int hit=-1;
    float best=1000.f;
    for(const auto& b:layout.buttons){
        const float dx=(nx-b.x)*width/size;
        const float dy=(ny-b.y)*height/size;
        const float dist=std::sqrt(dx*dx+dy*dy);
        if(dist<=b.r*1.23f && dist<best){
            best=dist;hit=(int)b.id;
        }
    }
    return hit;
}
} // namespace fitd_touch
