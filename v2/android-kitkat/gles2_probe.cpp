// FITD Android KitKat: SDL2 / GLES2 + physical Bluetooth/USB controller test.
// This is a hardware validation app, NOT the FITD gameplay engine.
// armv7a / Android API 19 / GLSL ES 1.00. No GLES3 functions or features.
#include <SDL.h>
#include <SDL_opengles2.h>
#include <cstdio>

namespace {
SDL_GameController* controller = nullptr;
SDL_Joystick* fallback = nullptr;

Sint32 activeInstance() {
    if (controller) return SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));
    if (fallback) return SDL_JoystickInstanceID(fallback);
    return -1;
}
void closePad() {
    if (controller) { SDL_GameControllerClose(controller); controller = nullptr; }
    if (fallback) { SDL_JoystickClose(fallback); fallback = nullptr; }
}
void openPad(int index) {
    if (controller || fallback || index < 0 || index >= SDL_NumJoysticks()) return;
    if (SDL_IsGameController(index)) {
        controller = SDL_GameControllerOpen(index);
        if (controller) {
            SDL_Log("FITD-ANDROID-PAD: GameController connected: %s",
                    SDL_GameControllerName(controller));
            return;
        }
    }
    // Older KitKat Bluetooth controllers sometimes expose only a raw joystick.
    fallback = SDL_JoystickOpen(index);
    if (fallback) {
        SDL_Log("FITD-ANDROID-PAD: raw joystick connected: %s",
                SDL_JoystickName(fallback));
    } else {
        SDL_Log("FITD-ANDROID-PAD: unable to open controller: %s", SDL_GetError());
    }
}
void findPad() {
    if (controller || fallback) return;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        openPad(i);
        if (controller || fallback) return;
    }
}
int axisX() {
    if (controller) return SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
    if (fallback && SDL_JoystickNumAxes(fallback) > 0) return SDL_JoystickGetAxis(fallback, 0);
    return 0;
}
int axisY() {
    if (controller) return SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
    if (fallback && SDL_JoystickNumAxes(fallback) > 1) return SDL_JoystickGetAxis(fallback, 1);
    return 0;
}
bool buttonA() {
    if (controller) return SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A);
    return fallback && SDL_JoystickNumButtons(fallback) > 0 &&
           SDL_JoystickGetButton(fallback, 0);
}
bool buttonExit() {
    if (controller) return SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_START)
        || SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B);
    return fallback && SDL_JoystickNumButtons(fallback) > 1 &&
           SDL_JoystickGetButton(fallback, 1);
}
bool dpad(SDL_GameControllerButton b, Uint8 hat) {
    if (controller) return SDL_GameControllerGetButton(controller, b);
    return fallback && SDL_JoystickNumHats(fallback) > 0 &&
        (SDL_JoystickGetHat(fallback, 0) & hat) != 0;
}
GLuint shader(GLenum kind, const char* source) {
    GLuint s = glCreateShader(kind);
    glShaderSource(s, 1, &source, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char msg[1024] = {};
        glGetShaderInfoLog(s, sizeof(msg), nullptr, msg);
        SDL_Log("FITD-ANDROID-GLES2: shader compile error: %s", msg);
        glDeleteShader(s);
        return 0;
    }
    return s;
}
} // namespace

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) != 0) {
        SDL_Log("FITD-ANDROID: SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    SDL_GameControllerEventState(SDL_ENABLE);
    SDL_JoystickEventState(SDL_ENABLE);
    findPad();
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_Window* win = SDL_CreateWindow("FITD GLES2 + Gamepad Test",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480,
        SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
    if (!win) {
        SDL_Log("FITD-ANDROID-GLES2: SDL_CreateWindow: %s", SDL_GetError());
        closePad(); SDL_Quit(); return 2;
    }
    SDL_GLContext context = SDL_GL_CreateContext(win);
    if (!context) {
        SDL_Log("FITD-ANDROID-GLES2: context: %s", SDL_GetError());
        SDL_DestroyWindow(win); closePad(); SDL_Quit(); return 3;
    }
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    SDL_Log("FITD-ANDROID-GLES2: GL_VERSION: %s", version ? version : "unknown");
    const char* vs =
        "attribute vec2 aPos; uniform vec2 uOffset; "
        "void main(){ gl_Position=vec4(aPos+uOffset,0.0,1.0); }";
    const char* fs =
        "precision mediump float; uniform vec3 uColor; "
        "void main(){ gl_FragColor=vec4(uColor,1.0); }";
    GLuint v = shader(GL_VERTEX_SHADER, vs), f = shader(GL_FRAGMENT_SHADER, fs);
    if (!v || !f) {
        if (v) glDeleteShader(v);
        if (f) glDeleteShader(f);
        SDL_GL_DeleteContext(context); SDL_DestroyWindow(win);
        closePad(); SDL_Quit(); return 4;
    }
    GLuint program = glCreateProgram();
    glAttachShader(program, v); glAttachShader(program, f);
    glBindAttribLocation(program, 0, "aPos");
    glLinkProgram(program);
    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        SDL_Log("FITD-ANDROID-GLES2: program link error");
        glDeleteProgram(program); glDeleteShader(v); glDeleteShader(f);
        SDL_GL_DeleteContext(context); SDL_DestroyWindow(win);
        closePad(); SDL_Quit(); return 5;
    }
    const GLfloat verts[] = {0.0f, 0.38f, -0.38f, -0.38f, 0.38f, -0.38f};
    GLuint buffer = 0;
    glGenBuffers(1, &buffer); glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    GLint uOffset = glGetUniformLocation(program, "uOffset");
    GLint uColor = glGetUniformLocation(program, "uColor");
    SDL_GL_SetSwapInterval(1);
    bool running = true;
    Uint32 started = SDL_GetTicks();
    float posX = 0.0f, posY = 0.0f;
    bool wasPressing = false;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT ||
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
                running = false;
            } else if (event.type == SDL_CONTROLLERDEVICEADDED ||
                       event.type == SDL_JOYDEVICEADDED) {
                openPad(event.type == SDL_CONTROLLERDEVICEADDED ?
                    event.cdevice.which : event.jdevice.which);
            } else if (event.type == SDL_CONTROLLERDEVICEREMOVED ||
                       event.type == SDL_JOYDEVICEREMOVED) {
                Sint32 removed = event.type == SDL_CONTROLLERDEVICEREMOVED ?
                    event.cdevice.which : event.jdevice.which;
                if (removed == activeInstance()) {
                    SDL_Log("FITD-ANDROID-PAD: disconnected");
                    closePad();
                    findPad();
                }
            }
        }
        // Polling also detects devices attached before SDL initialization.
        findPad();
        const int deadzone = 10000;
        int x = axisX(), y = axisY();
        float dx = (x < -deadzone ? -1.0f : x > deadzone ? 1.0f : 0.0f);
        float dy = (y < -deadzone ? -1.0f : y > deadzone ? 1.0f : 0.0f);
        if (dpad(SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_HAT_LEFT)) dx = -1.0f;
        if (dpad(SDL_CONTROLLER_BUTTON_DPAD_RIGHT, SDL_HAT_RIGHT)) dx = 1.0f;
        if (dpad(SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_HAT_UP)) dy = -1.0f;
        if (dpad(SDL_CONTROLLER_BUTTON_DPAD_DOWN, SDL_HAT_DOWN)) dy = 1.0f;
        posX += dx * 0.014f; posY -= dy * 0.014f;
        if (posX < -0.60f) posX = -0.60f;
        if (posX > 0.60f) posX = 0.60f;
        if (posY < -0.60f) posY = -0.60f;
        if (posY > 0.60f) posY = 0.60f;
        bool pressed = buttonA();
        if (pressed && !wasPressing) SDL_Log("FITD-ANDROID-PAD: A action detected");
        wasPressing = pressed;
        if (buttonExit()) {
            SDL_Log("FITD-ANDROID-PAD: exit button detected");
            running = false;
        }
        int width = 0, height = 0;
        SDL_GL_GetDrawableSize(win, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(program);
        glUniform2f(uOffset, posX, posY);
        glUniform3f(uColor, pressed ? 0.95f : 0.15f,
                            pressed ? 0.25f : 0.7f, 0.35f);
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        SDL_GL_SwapWindow(win);
        // Hardware validation app, not a game: auto-close after 90 seconds.
        if (SDL_GetTicks() - started > 90000) running = false;
        SDL_Delay(16);
    }
    glDeleteBuffers(1, &buffer); glDeleteProgram(program);
    glDeleteShader(v); glDeleteShader(f);
    SDL_GL_DeleteContext(context); SDL_DestroyWindow(win);
    closePad(); SDL_Quit();
    return 0;
}
