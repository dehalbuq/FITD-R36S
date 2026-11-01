// FITD Android KitKat: minimal SDL2 / GLES2 smoke test.
// No commercial game resources. Not the FITD engine.
#include <SDL.h>
#include <SDL_opengles2.h>
#include <cstdio>
#include <cstring>

static GLuint shader(GLenum kind, const char* source) {
    GLuint s=glCreateShader(kind);
    glShaderSource(s,1,&source,nullptr);
    glCompileShader(s);
    GLint ok=0; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok) { char log[1024]={}; glGetShaderInfoLog(s,sizeof(log),nullptr,log); SDL_Log("GLES2 shader: %s",log); glDeleteShader(s); return 0; }
    return s;
}
int main(int argc,char** argv) {
    (void)argc;(void)argv;
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER|SDL_INIT_AUDIO)!=0) return 1;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    SDL_Window* win=SDL_CreateWindow("FITD GLES2 Test",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,640,480,SDL_WINDOW_OPENGL|SDL_WINDOW_FULLSCREEN);
    if(!win) {SDL_Log("Window: %s",SDL_GetError());SDL_Quit();return 2;}
    SDL_GLContext context=SDL_GL_CreateContext(win);
    if(!context) {SDL_Log("Context: %s",SDL_GetError());SDL_DestroyWindow(win);SDL_Quit();return 3;}
    const char* version=(const char*)glGetString(GL_VERSION);
    SDL_Log("OpenGL ES version: %s",version?version:"unknown");
    const char* vs="attribute vec2 aPos; void main(){ gl_Position=vec4(aPos,0.0,1.0); }";
    const char* fs="precision mediump float; void main(){ gl_FragColor=vec4(0.15,0.7,0.35,1.0); }";
    GLuint v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,fs);
    if(!v||!f) {SDL_GL_DeleteContext(context);SDL_DestroyWindow(win);SDL_Quit();return 4;}
    GLuint program=glCreateProgram();glAttachShader(program,v);glAttachShader(program,f);glBindAttribLocation(program,0,"aPos");glLinkProgram(program);
    GLint linked=0;glGetProgramiv(program,GL_LINK_STATUS,&linked);
    if(!linked) {SDL_Log("GLES2 link failed");glDeleteProgram(program);glDeleteShader(v);glDeleteShader(f);SDL_GL_DeleteContext(context);SDL_DestroyWindow(win);SDL_Quit();return 5;}
    const GLfloat verts[]={0.0f,0.7f,-0.7f,-0.7f,0.7f,-0.7f};
    GLuint buffer=0;glGenBuffers(1,&buffer);glBindBuffer(GL_ARRAY_BUFFER,buffer);glBufferData(GL_ARRAY_BUFFER,sizeof(verts),verts,GL_STATIC_DRAW);
    SDL_GL_SetSwapInterval(1);
    bool running=true; Uint32 start=SDL_GetTicks();
    while(running) {
        SDL_Event event;
        while(SDL_PollEvent(&event)) {
            if(event.type==SDL_QUIT||event.type==SDL_KEYDOWN&&event.key.keysym.sym==SDLK_ESCAPE)running=false;
            if(event.type==SDL_CONTROLLERBUTTONDOWN&&event.cbutton.button==SDL_CONTROLLER_BUTTON_START)running=false;
        }
        int w=0,h=0;SDL_GL_GetDrawableSize(win,&w,&h);glViewport(0,0,w,h);
        glClearColor(0.05f,0.05f,0.1f,1.0f);glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(program);glBindBuffer(GL_ARRAY_BUFFER,buffer);
        glEnableVertexAttribArray(0);glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,nullptr);
        glDrawArrays(GL_TRIANGLES,0,3);SDL_GL_SwapWindow(win);
        if(SDL_GetTicks()-start>30000)running=false;
        SDL_Delay(1);
    }
    glDeleteBuffers(1,&buffer);glDeleteProgram(program);glDeleteShader(v);glDeleteShader(f);
    SDL_GL_DeleteContext(context);SDL_DestroyWindow(win);SDL_Quit();return 0;
}
