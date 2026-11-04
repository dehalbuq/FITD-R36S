// SDL2 GLES2 rendering of polygon geometry parsed from original FITD
// LISTBODY.PAK / LISTBOD2.PAK resources. Preview stage, NOT FitdMain gameplay.
#pragma once
#include "fitd_body_mesh.h"
#include <SDL.h>
#include <SDL_opengles2.h>
#include <array>

namespace fitd_character {
class ModelPreview {
public:
    bool init();
    bool select(int startIndex, int direction=1);
    void move(uint8_t joyMask);
    void turn(float angle);
    void draw();
    void shutdown();
    int currentIndex() const {return selectedIndex;}
private:
    bool loadFrom(const char* pak,int index,fitd_body::Mesh& mesh);
    bool tryIndex(int index);
    GLuint shader=0,vs=0,fs=0,vbo=0;
    fitd_body::Mesh mesh;
    std::array<float,768> palette{};
    int selectedIndex=-1;
    float actorX=0.f,actorY=-0.17f,rotation=0.3f;
};
} // namespace fitd_character
