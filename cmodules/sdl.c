#include <tl.h>
#include <SDL2/SDL.h>

#include "trace-off.h"

TL_REF_TYPE(sdlWindow);
tlKind* sdlWindowKind;

struct sdlWindow {
    SDL_Window* window;
};

static tlHandle _Window_new(tlTask* task, tlArgs* args) {
    tlString* title = tlStringCast(tlArgsGet(args, 0));
    int width = tl_int(tlArgsGet(args, 1));
    int height = tl_int(tlArgsGet(args, 2));

    sdlWindow* w = tlAlloc(sdlWindowKind, sizeof(sdlWindow));
    w->window = SDL_CreateWindow(tlStringData(title), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_OPENGL);
    return w;
}

static tlHandle _window_close(tlTask* task, tlArgs* args) {
    TL_TARGET(sdlWindow, w);
    SDL_DestroyWindow(w->window);
    return tlNull;
}

tlObject* sdlStatic;

tlHandle tl_load() {
    if (sdlStatic) return sdlStatic;

    SDL_Init(SDL_INIT_EVERYTHING);

    tlKind _sdlWindowKind = { .name = "Window", .locked = true };
    INIT_KIND(sdlWindowKind);

    tlClass* cls = tlCLASS("Window", null,
    tlMETHODS(
        "close", _window_close,
        null
    ), tlMETHODS(
        "new", _Window_new,
        null
    ));
    sdlWindowKind->cls = cls;

    sdlStatic = tlObjectFrom(
        "Window", cls,
        null
    );
    return sdlStatic;
}

