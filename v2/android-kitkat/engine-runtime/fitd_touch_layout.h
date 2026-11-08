// Shared screen-space geometry used by FITD GLES2 HUD and SDL touch input.
// Normalized coordinates have origin at the TOP-LEFT; preserve the 4:3 game.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace fitd_touch {
enum ButtonId { UP, DOWN, LEFT, RIGHT, ACTION, BACK, CONFIRM, MENU, COUNT };
struct Button {
    ButtonId id;
    float x,y,r; // fractions of screen width/height; r is screen MIN dimension
    const char* label;
};
struct Layout {
    bool active=false;
    bool portrait=false;
    int x=0,y=0,w=0,h=0; // game viewport in drawable coords with bottom-left origin
    std::array<Button,COUNT> buttons{};
};
inline Layout compute(int width,int height,bool touchscreen) {
    Layout out;
    if(width<1 || height<1)return out;
    out.active=touchscreen;
    out.portrait=(height>width);
    float areaW=(float)width;
    float areaH=(float)height;
    float regionTop=0.f;
    if(touchscreen){
        if(out.portrait){
            areaW=width*0.97f;
            areaH=height*0.56f;
        }else {
            areaW=width*0.52f;
            areaH=height*0.94f;
        }
    }
    float gameW=std::min(areaW,areaH*4.f/3.f);
    float gameH=gameW*3.f/4.f;
    out.w=std::max(1,(int)std::floor(gameW));
    out.h=std::max(1,(int)std::floor(gameH));
    out.x=(width-out.w)/2;
    const float top=(areaH-gameH)*0.5f+regionTop;
    out.y=height-(int)std::floor(top)-out.h;
    if(!touchscreen)return out;
    // Keep all hit targets clear of the 4:3 game viewport.
    if(out.portrait){
        const float rad=0.077f;
        out.buttons={{
            {UP, .23f,.682f,rad,"U"},
            {DOWN,.23f,.834f,rad,"D"},
            {LEFT,.10f,.758f,rad,"L"},
            {RIGHT,.36f,.758f,rad,"R"},
            {ACTION,.77f,.835f,rad,"A"},
            {BACK,.90f,.758f,rad,"B"},
            {CONFIRM,.77f,.681f,rad,"Y"},
            {MENU,.63f,.758f,rad,"M"},
        }};
    }else{
        const float rad=0.079f;
        const float shortRatio=(float)height/(float)width;
        const float dx=0.127f*shortRatio;
        out.buttons={{
            {UP,.14f,.446f,rad,"U"},
            {DOWN,.14f,.714f,rad,"D"},
            {LEFT,.14f-dx,.58f,rad,"L"},
            {RIGHT,.14f+dx,.58f,rad,"R"},
            {ACTION,.86f,.715f,rad,"A"},
            {BACK,.86f+dx,.58f,rad,"B"},
            {CONFIRM,.86f,.445f,rad,"Y"},
            {MENU,.86f-dx,.58f,rad,"M"},
        }};
    }
    return out;
}
inline int hit(int width,int height,float nx,float ny) {
    const Layout l=compute(width,height,true);
    // Touch buttons have circular hit areas with slightly generous margins.
    const float minSize=(float)std::min(width,height);
    int best=-1;
    float bestD=1000.f;
    for(const auto& b:l.buttons){
        const float dx=(nx-b.x)*width/minSize;
        const float dy=(ny-b.y)*height/minSize;
        const float distance=std::sqrt(dx*dx+dy*dy);
        if(distance<=b.r*1.14f && distance<bestD){
            best=(int)b.id;
            bestD=distance;
        }
    }
    return best;
}
} // namespace fitd_touch
