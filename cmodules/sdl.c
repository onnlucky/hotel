#include <tl.h>
#include <SDL2/SDL.h>

#include "trace-off.h"

TL_REF_TYPE(sdlWindow);
tlKind* sdlWindowKind;

TL_REF_TYPE(sdlEvent);
tlKind* sdlEventKind;

struct sdlWindow {
    tlLock* head;
    SDL_Window* window;
};

struct sdlEvent {
    tlHead* head;
    SDL_Event event;
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

static tlHandle _event(tlTask* task, tlArgs* args) {
    SDL_Event event;
    if (tl_bool(tlArgsGet(args, 0))) {
        if (!SDL_PollEvent(&event)) return tlNull;
    } else {
        if (!SDL_WaitEvent(&event)) return tlNull;
    }

    sdlEvent* e = tlAlloc(sdlEventKind, sizeof(sdlEvent));
    e->event = event;
    return e;
}

static tlHandle _event_type(tlTask* task, tlArgs* args) {
    TL_TARGET(sdlEvent, e);
    return tlINT(e->event.type);
}

tlObject* sdlStatic;

tlHandle tl_load() {
    if (sdlStatic) return sdlStatic;

    SDL_Init(SDL_INIT_EVERYTHING);

    tlKind _sdlWindowKind = { .name = "Window", .locked = true };
    INIT_KIND(sdlWindowKind);
    tlClass* windowcls = tlCLASS("Window", null,
    tlMETHODS(
        "close", _window_close,
        null
    ), tlMETHODS(
        "new", _Window_new,
        null
    ));
    sdlWindowKind->cls = windowcls;

    tlKind _sdlEventKind = { .name = "Event" };
    INIT_KIND(sdlEventKind);
    tlClass* eventcls = tlCLASS("Event", null,
    tlMETHODS(
        "type", _event_type,
        null
    ), null);
    sdlEventKind->cls = eventcls;


    sdlStatic = tlObjectFrom(
        "Window", windowcls,
        "event", tlNATIVE(_event, "event"),
        null
    );
    return sdlStatic;
}

