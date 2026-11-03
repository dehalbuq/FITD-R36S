// FITD-R36S Android 4.4+ ARMv7 diagnostic — not the commercial game.
// Runs SDL2 GLES2 video, dumps SDL joystick/gamecontroller/keyboard events,
// GL limits, audio drivers. Java activity exports this log alongside Android
// InputDevice and raw KeyEvent/MotionEvent descriptions to one text file.
#include <SDL.h>
#include <SDL_opengles2.h>
#include <SDL_system.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

namespace {
FILE* logFile = nullptr;
SDL_LogOutputFunction previousLog = nullptr;
void* previousUserdata = nullptr;
size_t bytesLogged = 0;

void SDLCALL diagnosticLog(void* userdata, int category,
                          SDL_LogPriority priority, const char* message) {
    if (previousLog) previousLog(previousUserdata, category, priority, message);
    if (logFile && bytesLogged < 1500000 && message) {
        int n = fprintf(logFile, "%u [SDL-%d] %s\n",
                        static_cast<unsigned>(SDL_GetTicks()), category, message);
        if (n > 0) bytesLogged += static_cast<size_t>(n);
        fflush(logFile);
    }
}
void enableReport() {
    const char* dir = SDL_AndroidGetExternalStoragePath();
    if (!dir) dir = SDL_AndroidGetInternalStoragePath();
    if (!dir) return;
    char path[1024] = {};
    snprintf(path, sizeof(path), "%s/fitd-sdl2-input.txt", dir);
    logFile = fopen(path, "w");
    SDL_LogGetOutputFunction(&previousLog, &previousUserdata);
    SDL_LogSetOutputFunction(diagnosticLog, nullptr);
    SDL_Log("FITD-DIAG SDL2 report started. path=%s", path);
}
void finishReport() {
    SDL_Log("FITD-DIAG SDL2 report ended");
    SDL_LogSetOutputFunction(previousLog, previousUserdata);
    if (logFile) { fclose(logFile); logFile = nullptr; }
}
void enumerateSDL() {
    int num = SDL_NumJoysticks();
    SDL_Log("FITD-DIAG SDL_NUM_JOYSTICKS=%d", num);
    for (int i=0; i<num; ++i) {
        SDL_JoystickGUID g = SDL_JoystickGetDeviceGUID(i);
        char guid[40] = {};
        SDL_JoystickGetGUIDString(g, guid, sizeof(guid));
        SDL_Log("FITD-DIAG SDL_DEVICE index=%d instance=%d name=%s "
                "vendor=%u product=%u isGameController=%d GUID=%s",
            i, SDL_JoystickGetDeviceInstanceID(i),
            SDL_JoystickNameForIndex(i) ? SDL_JoystickNameForIndex(i) : "(unknown)",
            (unsigned)SDL_JoystickGetDeviceVendor(i),
            (unsigned)SDL_JoystickGetDeviceProduct(i),
            SDL_IsGameController(i), guid);
        SDL_Joystick* j = SDL_JoystickOpen(i);
        if (j) {
            SDL_Log("FITD-DIAG SDL_AXES device=%d axes=%d hats=%d buttons=%d balls=%d",
              SDL_JoystickInstanceID(j), SDL_JoystickNumAxes(j),
              SDL_JoystickNumHats(j), SDL_JoystickNumButtons(j),
              SDL_JoystickNumBalls(j));
            SDL_JoystickClose(j);
        }
        if (SDL_IsGameController(i)) {
            char* map = SDL_GameControllerMappingForDeviceIndex(i);
            if (map) {
                SDL_Log("FITD-DIAG SDL_GAMECONTROLLER_MAPPING index=%d %s", i, map);
                SDL_free(map);
            }
        }
    }
}
void dumpAudio() {
    int ok = SDL_InitSubSystem(SDL_INIT_AUDIO);
    SDL_Log("FITD-DIAG AUDIO init=%d currentDriver=%s",
        ok, SDL_GetCurrentAudioDriver() ? SDL_GetCurrentAudioDriver() : "(none)");
    int n = SDL_GetNumAudioDrivers();
    for (int i=0; i<n; ++i)
        SDL_Log("FITD-DIAG AUDIO_DRIVER %d %s", i, SDL_GetAudioDriver(i));
    if (ok == 0) {
        int outputs = SDL_GetNumAudioDevices(0);
        SDL_Log("FITD-DIAG AUDIO_OUTPUTS=%d", outputs);
        for (int i=0; i<outputs && i<8; ++i)
            SDL_Log("FITD-DIAG AUDIO_OUTPUT %d %s", i, SDL_GetAudioDeviceName(i, 0));
    }
}
void dumpGL() {
    const char* k[] = {"GL_VENDOR", "GL_RENDERER", "GL_VERSION", "GL_SHADING_LANGUAGE_VERSION"};
    GLenum ids[] = {GL_VENDOR, GL_RENDERER, GL_VERSION, GL_SHADING_LANGUAGE_VERSION};
    for (int i=0; i<4; ++i) {
        const char* value = reinterpret_cast<const char*>(glGetString(ids[i]));
        SDL_Log("FITD-DIAG %s=%s", k[i], value ? value : "(null)");
    }
    GLint v=0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE,&v); SDL_Log("FITD-DIAG GL_MAX_TEXTURE_SIZE=%d",v);
    glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE,&v); SDL_Log("FITD-DIAG GL_MAX_RENDERBUFFER_SIZE=%d",v);
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS,&v); SDL_Log("FITD-DIAG GL_MAX_VERTEX_ATTRIBS=%d",v);
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS,&v); SDL_Log("FITD-DIAG GL_MAX_TEXTURE_IMAGE_UNITS=%d",v);
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS,&v); SDL_Log("FITD-DIAG GL_MAX_VERTEX_UNIFORM_VECTORS=%d",v);
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS,&v); SDL_Log("FITD-DIAG GL_MAX_FRAGMENT_UNIFORM_VECTORS=%d",v);
    glGetIntegerv(GL_MAX_VARYING_VECTORS,&v); SDL_Log("FITD-DIAG GL_MAX_VARYING_VECTORS=%d",v);
    int major=0,minor=0,depth=0;
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,&major);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,&minor);
    SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE,&depth);
    SDL_Log("FITD-DIAG SDL_GL_CONTEXT=%d.%d depth=%d",major,minor,depth);
}
GLuint makeShader(GLenum kind, const char* source) {
    GLuint s = glCreateShader(kind);
    glShaderSource(s,1,&source,nullptr);
    glCompileShader(s);
    GLint valid=0;
    glGetShaderiv(s,GL_COMPILE_STATUS,&valid);
    if (!valid) {
        char error[1024] = {};
        glGetShaderInfoLog(s,sizeof(error),nullptr,error);
        SDL_Log("FITD-DIAG GLES2 shader error %s",error);
        glDeleteShader(s);
        return 0;
    }
    return s;
}
} // namespace

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK|SDL_INIT_GAMECONTROLLER) < 0) {
        SDL_Log("FITD-DIAG: SDL_Init error: %s",SDL_GetError());
        return 1;
    }
    enableReport();
    SDL_Log("FITD-DIAG build=ARMv7 API19 GLES2 SDL version %u.%u.%u",
            SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL);
    SDL_Log("FITD-DIAG SDL_PLATFORM=%s CPU_COUNT=%d SYSTEM_RAM_MB=%d",
            SDL_GetPlatform(), SDL_GetCPUCount(), SDL_GetSystemRAM());
    SDL_GameControllerEventState(SDL_ENABLE);
    SDL_JoystickEventState(SDL_ENABLE);
    enumerateSDL();
    dumpAudio();

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    SDL_Window* window=SDL_CreateWindow("FITD ARMv7 Diagnostic",
      SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,640,480,
      SDL_WINDOW_OPENGL|SDL_WINDOW_FULLSCREEN);
    if (!window) {
        SDL_Log("FITD-DIAG WINDOW_FAIL=%s",SDL_GetError());
        finishReport();SDL_Quit();return 2;
    }
    SDL_GLContext glcontext=SDL_GL_CreateContext(window);
    if (!glcontext) {
        SDL_Log("FITD-DIAG GLES2_CONTEXT_FAIL=%s",SDL_GetError());
        SDL_DestroyWindow(window);finishReport();SDL_Quit();return 3;
    }
    dumpGL();
    if (SDL_GL_SetSwapInterval(1) != 0) SDL_Log("FITD-DIAG VSYNC unavailable %s",SDL_GetError());

    const char* vsrc="attribute vec2 pos; uniform vec2 offset; "
                     "void main(){gl_Position=vec4(pos+offset,0.0,1.0);}";
    const char* fsrc="precision mediump float; uniform vec3 rgb; "
                     "void main(){gl_FragColor=vec4(rgb,1.0);}";
    GLuint vs=makeShader(GL_VERTEX_SHADER,vsrc), fs=makeShader(GL_FRAGMENT_SHADER,fsrc);
    if (!vs||!fs) {
        if (vs) glDeleteShader(vs);if (fs) glDeleteShader(fs);
        SDL_GL_DeleteContext(glcontext);SDL_DestroyWindow(window);
        finishReport();SDL_Quit();return 4;
    }
    GLuint program=glCreateProgram();
    glAttachShader(program,vs);glAttachShader(program,fs);
    glBindAttribLocation(program,0,"pos");glLinkProgram(program);
    GLint linked=0;glGetProgramiv(program,GL_LINK_STATUS,&linked);
    if (!linked) {
        SDL_Log("FITD-DIAG GLES2 program link failed");
        glDeleteProgram(program);glDeleteShader(vs);glDeleteShader(fs);
        SDL_GL_DeleteContext(glcontext);SDL_DestroyWindow(window);
        finishReport();SDL_Quit();return 5;
    }
    GLuint vertices=0;const float triangle[]={-0.26f,-0.24f,0.26f,-0.24f,0.f,0.24f};
    glGenBuffers(1,&vertices);glBindBuffer(GL_ARRAY_BUFFER,vertices);
    glBufferData(GL_ARRAY_BUFFER,sizeof(triangle),triangle,GL_STATIC_DRAW);
    GLint locOffset=glGetUniformLocation(program,"offset");
    GLint locColor=glGetUniformLocation(program,"rgb");
    Uint32 lastPoll=0,lastAxisLog=0,lastStatus=0,started=SDL_GetTicks();
    bool running=true, keyboardSeen=false, action=false;
    float cx=0.f,cy=0.f;
    int deviceUpdates=0;
    while (running && SDL_GetTicks()-started<600000u) {
        SDL_Event e;
        while(SDL_PollEvent(&e)) {
            switch(e.type) {
                case SDL_QUIT: running=false;break;
                case SDL_KEYDOWN:
                case SDL_KEYUP:
                    SDL_Log("FITD-DIAG SDL_KEY %s which=%u scancode=%d sym=%d name=%s repeat=%d",
                        e.type==SDL_KEYDOWN?"DOWN":"UP",
                        (unsigned)e.key.windowID,(int)e.key.keysym.scancode,
                        (int)e.key.keysym.sym, SDL_GetKeyName(e.key.keysym.sym),
                        (int)e.key.repeat);
                    keyboardSeen=true;
                    if (e.type==SDL_KEYDOWN && e.key.keysym.sym==SDLK_ESCAPE)
                        SDL_Log("FITD-DIAG escape received; log preserved");
                    break;
                case SDL_CONTROLLERDEVICEADDED:
                case SDL_JOYDEVICEADDED:
                case SDL_CONTROLLERDEVICEREMOVED:
                case SDL_JOYDEVICEREMOVED:
                case SDL_CONTROLLERDEVICEREMAPPED:
                    SDL_Log("FITD-DIAG SDL_DEVICE_EVENT type=%u data=%d",
                        (unsigned)e.type,(int)e.jdevice.which);
                    deviceUpdates++;
                    enumerateSDL();
                    break;
                case SDL_CONTROLLERBUTTONDOWN:
                case SDL_CONTROLLERBUTTONUP:
                    SDL_Log("FITD-DIAG SDL_CONTROLLER_BUTTON dev=%d button=%u %s",
                            (int)e.cbutton.which,(unsigned)e.cbutton.button,
                            e.type==SDL_CONTROLLERBUTTONDOWN?"DOWN":"UP");
                    break;
                case SDL_JOYBUTTONDOWN:
                case SDL_JOYBUTTONUP:
                    SDL_Log("FITD-DIAG SDL_JOYSTICK_BUTTON dev=%d button=%u %s",
                           (int)e.jbutton.which,(unsigned)e.jbutton.button,
                           e.type==SDL_JOYBUTTONDOWN?"DOWN":"UP");
                    break;
                case SDL_JOYHATMOTION:
                    SDL_Log("FITD-DIAG SDL_HAT dev=%d hat=%u value=%u",
                        (int)e.jhat.which,(unsigned)e.jhat.hat,(unsigned)e.jhat.value);
                    break;
                case SDL_JOYAXISMOTION:
                    if ((e.jaxis.value>12000||e.jaxis.value< -12000) &&
                            SDL_GetTicks()-lastAxisLog>180) {
                        lastAxisLog=SDL_GetTicks();
                        SDL_Log("FITD-DIAG SDL_JOYSTICK_AXIS dev=%d axis=%u value=%d",
                            (int)e.jaxis.which,(unsigned)e.jaxis.axis,(int)e.jaxis.value);
                    }
                    break;
                case SDL_CONTROLLERAXISMOTION:
                    if ((e.caxis.value>12000||e.caxis.value< -12000) &&
                            SDL_GetTicks()-lastAxisLog>180) {
                        lastAxisLog=SDL_GetTicks();
                        SDL_Log("FITD-DIAG SDL_CONTROLLER_AXIS dev=%d axis=%u value=%d",
                            (int)e.caxis.which,(unsigned)e.caxis.axis,(int)e.caxis.value);
                    }
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    SDL_Log("FITD-DIAG SDL_MOUSE_BUTTON button=%u x=%d y=%d",
                            (unsigned)e.button.button,e.button.x,e.button.y);break;
                default: break;
            }
        }
        // Detect devices the Android firmware added after opening the app.
        if (SDL_GetTicks()-lastPoll>3000) {lastPoll=SDL_GetTicks();enumerateSDL();}
        float dx=0.f,dy=0.f;action=false;
        const Uint8* keys=SDL_GetKeyboardState(nullptr);
        if (keys[SDL_SCANCODE_LEFT]||keys[SDL_SCANCODE_A]) dx=-1;
        if (keys[SDL_SCANCODE_RIGHT]||keys[SDL_SCANCODE_D]) dx=1;
        if (keys[SDL_SCANCODE_UP]||keys[SDL_SCANCODE_W]) dy=-1;
        if (keys[SDL_SCANCODE_DOWN]||keys[SDL_SCANCODE_S]) dy=1;
        if (keys[SDL_SCANCODE_SPACE]||keys[SDL_SCANCODE_RETURN])action=true;
        int count=SDL_NumJoysticks();
        // Poll EVERY enumerated joystick: do not let the projector remote
        // monopolize slot zero. Record mapping per device instead.
        for(int i=0;i<count;i++){
            SDL_Joystick* joy=SDL_JoystickFromInstanceID(SDL_JoystickGetDeviceInstanceID(i));
            if (!joy) joy=SDL_JoystickOpen(i);
            if (!joy) continue;
            if(SDL_JoystickNumAxes(joy)>=2){
                Sint16 x=SDL_JoystickGetAxis(joy,0),y=SDL_JoystickGetAxis(joy,1);
                if(x< -9000)dx=-1; else if(x>9000)dx=1;
                if(y< -9000)dy=-1; else if(y>9000)dy=1;
            }
            if(SDL_JoystickNumHats(joy)>0) {
                Uint8 h=SDL_JoystickGetHat(joy,0);
                if(h&SDL_HAT_LEFT)dx=-1;if(h&SDL_HAT_RIGHT)dx=1;
                if(h&SDL_HAT_UP)dy=-1;if(h&SDL_HAT_DOWN)dy=1;
            }
            if(SDL_JoystickNumButtons(joy)>0 && SDL_JoystickGetButton(joy,0))
                action=true;
        }
        cx += dx*0.012f;cy -= dy*0.012f;
        if (cx>0.65f)cx=0.65f;if(cx< -0.65f)cx=-0.65f;
        if (cy>0.55f)cy=0.55f;if(cy< -0.55f)cy=-0.55f;
        int w=0,h=0;SDL_GL_GetDrawableSize(window,&w,&h);
        glViewport(0,0,w,h);
        glClearColor(0.04f,0.05f,0.12f,1.0f);glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(program);glUniform2f(locOffset,cx,cy);
        glUniform3f(locColor,action?0.9f:0.17f,action?0.2f:0.75f,0.32f);
        glBindBuffer(GL_ARRAY_BUFFER,vertices);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,nullptr);
        glDrawArrays(GL_TRIANGLES,0,3);
        // SDL status: green if at least one SDL controller / yellow if raw
        // joystick / red if zero. Purple shows SDL keyboard events observed.
        bool hasPad=false,hasJoystick=false;
        for(int i=0;i<SDL_NumJoysticks();++i) {
            if(SDL_IsGameController(i))hasPad=true;
            else hasJoystick=true;
        }
        glEnable(GL_SCISSOR_TEST);
        if(w>128&&h>100){
            glScissor(12,h-70,50,50);
            if(hasPad)glClearColor(0.1f,0.8f,0.3f,1);
            else if(hasJoystick)glClearColor(0.95f,0.8f,0.1f,1);
            else glClearColor(0.85f,0.15f,0.15f,1);
            glClear(GL_COLOR_BUFFER_BIT);
            if(keyboardSeen){
                glScissor(w-62,h-70,50,50);
                glClearColor(0.6f,0.15f,0.85f,1);
                glClear(GL_COLOR_BUFFER_BIT);
            }
        }
        glDisable(GL_SCISSOR_TEST);
        SDL_GL_SwapWindow(window);
        if(SDL_GetTicks()-lastStatus>5000){
            lastStatus=SDL_GetTicks();
            SDL_Log("FITD-DIAG SDL_STATUS joys=%d pad=%d raw=%d keyboardSeen=%d "
                    "position=%0.3f,%0.3f action=%d deviceEvents=%d",
                    count,hasPad,hasJoystick,keyboardSeen,cx,cy,action,deviceUpdates);
        }
        SDL_Delay(16);
    }
    glDeleteBuffers(1,&vertices);glDeleteProgram(program);
    glDeleteShader(vs);glDeleteShader(fs);
    SDL_GL_DeleteContext(glcontext);SDL_DestroyWindow(window);
    finishReport();SDL_Quit();return 0;
}
