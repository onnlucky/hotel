#ifndef _window_h_
#define _window_h_

#include "vm/tl.h"
#include <cairo/cairo.h>

TL_REF_TYPE(Box);
TL_REF_TYPE(Window);

void window_init(tlVm* vm);

// platform integration

typedef void NativeWindow;

struct Window {
    tlLock lock;
    tlTask* rendertask;
    NativeWindow* native;
    Box* root;

    bool dirty;
    int x; int y; int width; int height;

    tlHandle onkey;
    tlHandle onresize;
};

void renderWindow(Window* window, cairo_t* cairo);

// platform needs to implement this
NativeWindow* nativeWindowNew(Window* w);

void nativeWindowClose(NativeWindow*);
void nativeWindowFocus(NativeWindow*); // requesting focus makes window visible too
void nativeWindowSetVisible(NativeWindow*, bool visible);
int nativeWindowVisible(NativeWindow*);

void nativeWindowRedraw(NativeWindow* w);

void nativeWindowFrame(NativeWindow*, int* x, int* y, int* width, int* height);
void nativeWindowSetPos(NativeWindow*, int x, int y);
void nativeWindowSetSize(NativeWindow*, int width, int height);

tlString* nativeWindowTitle(NativeWindow*);
void nativeWindowSetTitle(NativeWindow*, tlString* title);

// platform can call the following for callbacks
void windowPointerEvent(Window* w);
void windowKeyEvent(Window* w, int code, tlString* input);

#endif // _window_h_
