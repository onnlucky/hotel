#import <Cocoa/Cocoa.h>
#include <pthread.h>
#include <cairo/cairo.h>
#include <cairo/cairo-quartz.h>

#include "../tl.h"

#include "graphics.h"

static void ns_init();

TL_REF_TYPE(App);
TL_REF_TYPE(Window);

@interface HotelView: NSView {
@public
    Window* window;
} @end
@interface HotelWindow: NSWindow {
@public
    Window* window;
} @end

struct App {
    tlHead head;
    NSApplication* app;
};
static tlClass _AppClass;
tlClass* AppClass = &_AppClass;
static App* shared;

static tlValue App_shared(tlTask* task, tlArgs* args) {
    ns_init();
    assert(shared);
    return shared;
}

struct Window {
    tlHead head;
    HotelWindow* nswindow;
    Graphics* buf;
    Graphics* draw;
    int width;
    int height;
};
static tlClass _WindowClass;
tlClass* WindowClass = &_WindowClass;

static tlValue _Window_new(tlTask* task, tlArgs* args) {
    ns_init();
    Window* window = tlAlloc(task, WindowClass, sizeof(Window));
    window->nswindow = [[HotelWindow alloc]
            initWithContentRect: NSMakeRect(0, 0, 200, 200)
                      styleMask: NSResizableWindowMask|NSClosableWindowMask|NSTitledWindowMask
                        backing: NSBackingStoreBuffered
                          defer: NO];
    HotelView* view = [[HotelView new] autorelease];
    view->window = window;
    [window->nswindow setContentView: view];
    window->nswindow->window = window;
    return window;
}

static tlValue _window_show(tlTask* task, tlArgs* args) {
    Window* window = WindowAs(tlArgsTarget(args));
    [window->nswindow makeKeyAndOrderFront: nil];
    return tlNull;
}
static tlValue _window_hide(tlTask* task, tlArgs* args) {
    Window* window = WindowAs(tlArgsTarget(args));
    [window->nswindow orderOut: nil];
    return tlNull;
}
static tlValue _window_graphics(tlTask* task, tlArgs* args) {
    Window* window = WindowAs(tlArgsTarget(args));
    Graphics* buf = buf;
    window->buf = null;
    return graphicsSizeTo(task, buf, window->width, window->height);
}
static tlValue _window_draw(tlTask* task, tlArgs* args) {
    Window* window = WindowAs(tlArgsTarget(args));
    Graphics* buf = GraphicsCast(tlArgsAt(args, 0));
    if (!buf) TL_THROW("expected a buffer");
    if (a_swap_if(A_VAR(window->draw), A_VAL(buf), null) != null) {
        // TODO block the current task until drawing is ready ...
        fatal("not implemented yet");
    }

    [[window->nswindow contentView]
            performSelectorOnMainThread: @selector(needsDisplay)
                             withObject: nil waitUntilDone: NO];
    return tlNull;
}

@implementation HotelWindow
@end

@implementation HotelView

- (void)drawRect: (NSRect)rect {
    Graphics* buf = A_PTR(a_swap(A_VAR(window->draw), null));
    if (!buf) {
        warning("not buffer to draw... why did you call needsDisplay?");
        return;
    }

    // get gc context and translate/scale it for cairo
    CGContextRef ctx = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
    CGContextTranslateCTM(ctx, 0.0, window->height);
    CGContextScaleCTM(ctx, 1.0, -1.0);

    // create a cairo context and drow the buf context to it
    cairo_surface_t *surface =
            cairo_quartz_surface_create_for_cg_context(ctx, window->width, window->height);
    cairo_t *cr = cairo_create(surface);

    graphicsDrawOn(buf, cr);

    // pop the group, paint it and cleanup
    cairo_paint(cr);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

@end

void window_init(tlVm* vm) {
    // TODO if we do this, will will need to drain it sometime ...
    [NSAutoreleasePool new];
    _WindowClass.map = tlClassMapFrom(
        "show", _window_show,
        "hide", _window_hide,
        "graphics", _window_graphics,
        "draw", _window_draw,
        null
    );
    tlMap* WindowStatic = tlClassMapFrom(
        "new", _Window_new,
        null
    );
    tlVmGlobalSet(vm, tlSYM("Window"), WindowStatic);
}

// this code launches a second thread to run hotel in, maybe starting the Cocoa framework in the first

static bool should_start_cocoa;
static pthread_mutex_t cocoa;
static pthread_cond_t cocoa_start;

static void ns_init() {
    static bool inited;
    print("NS INITED: %s", inited?"true":"false");
    if (inited) return; inited = true;

    print("locking");
    pthread_mutex_lock(&cocoa);
    should_start_cocoa = true;
    print("signalling");
    pthread_cond_signal(&cocoa_start);
    print("waiting");
    pthread_cond_wait(&cocoa_start, &cocoa);
    pthread_mutex_unlock(&cocoa);
    print("done...: %p", NSApp);

    shared = tlAlloc(null, AppClass, sizeof(App));
    shared->app = NSApp;
}

static void ns_stop() {
    pthread_mutex_lock(&cocoa);
    if (should_start_cocoa) {
        [[NSApplication sharedApplication] performSelectorOnMainThread: @selector(stop:) withObject: nil waitUntilDone: NO];
        // TODO this fixes the above stop waiting until first event in queue actually stops the loop
        // we need to post an event in the queue and wake it up
        [[NSApplication sharedApplication] performSelectorOnMainThread: @selector(terminate:) withObject: nil waitUntilDone: NO];
        NSLog(@"stop app: %@", NSApp);
    } else {
        pthread_cond_signal(&cocoa_start);
    }
    pthread_mutex_unlock(&cocoa);
}

static void* tl_main(void* data) {
    tl_init();
    tlVm* vm = tlVmNew();
    tlVmInitDefaultEnv(vm);
    graphics_init(vm);
    window_init(vm);
    tlVmRunFile(vm, tlTEXT("run.tl"));
    ns_stop();
    return 0;
}

pthread_t tl_thread;
int main() {
    pthread_mutex_init(&cocoa, null);
    pthread_cond_init(&cocoa_start, null);

    pthread_mutex_lock(&cocoa);

    pthread_create(&tl_thread, null, &tl_main, null);
    pthread_cond_wait(&cocoa_start, &cocoa);
    bool should_start = should_start_cocoa;

    pthread_mutex_unlock(&cocoa);

    if (should_start) {
        [NSAutoreleasePool new];
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy: NSApplicationActivationPolicyRegular];
        assert([NSThread isMainThread]);
        pthread_cond_signal(&cocoa_start);
        [NSApp run];
    }

    pthread_join(tl_thread, null);
    return 0;
}

