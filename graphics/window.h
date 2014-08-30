#ifndef _window_h_
#define _window_h_

#include "vm/tl.h"
#include <cairo/cairo.h>

TL_REF_TYPE(Box);
TL_REF_TYPE(Text);
TL_REF_TYPE(Window);

void window_init(tlVm* vm);

// platform integration

typedef void NativeWindow;
typedef void NativeTextBox;

struct Window {
    tlLock lock;
    tlTask* rendertask;
    NativeWindow* native;
    Box* root;

    bool dirty;
    int x; int y; int width; int height;

    tlHandle onkey;
    tlHandle onmouse;
    tlHandle onmousemove;
    tlHandle onmousescroll;
    tlHandle onresize;
};

void renderWindow(Window* window, cairo_t* cairo);
void closeWindow(Window* window);

// platform needs to implement this

void nativeClipboardSet(tlString*);
tlString* nativeClipboardGet();

NativeWindow* nativeWindowNew(Window* w);

void nativeWindowClose(NativeWindow*);
void nativeWindowFocus(NativeWindow*); // requesting focus makes window visible too
void nativeWindowSetVisible(NativeWindow*, bool visible);
int nativeWindowVisible(NativeWindow*);

void nativeWindowRedraw(NativeWindow* w);

void nativeWindowFrame(NativeWindow*, int* x, int* y, int* width, int* height);
void nativeWindowSetPos(NativeWindow*, int x, int y);
void nativeWindowSetSize(NativeWindow*, int width, int height);
bool nativeWindowFullScreen(NativeWindow*);
void nativeWindowSetFullScreen(NativeWindow*, bool full);

tlString* nativeWindowTitle(NativeWindow*);
void nativeWindowSetTitle(NativeWindow*, tlString* title);

// platform can call the following for callbacks
void windowMouseEvent(Window* w, double x, double y, int buttons, int count, int modifiers);
void windowMouseMoveEvent(Window* w, double x, double y, int buttons, int modifiers);
void windowMouseScrollEvent(Window* w, double deltaX, double deltaY);
void windowKeyEvent(Window* w, int code, tlString* input, int modifiers);
void windowResizeEvent(Window* w, int x, int y, int width, int height);

// textbox support
NativeTextBox* nativeTextBoxNew(NativeWindow*, int x, int y, int width, int height);
void nativeTextBoxPosition(NativeTextBox*, int x, int y, int with, int height);
tlString* nativeTextBoxGetText(NativeTextBox*);

#endif // _window_h_

