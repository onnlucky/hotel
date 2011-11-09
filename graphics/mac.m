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
static tlClass _AppClass = {
    .name = "App",
};
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
    Graphics* old;
};
static tlClass _WindowClass = {
    .name = "Window"
};
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
    [window->nswindow center];
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
    NSRect frame = [[window->nswindow contentView] frame];
    Graphics* buf = A_PTR(a_swap(A_VAR(window->buf), null));
    Graphics* g = graphicsSizeTo(task, buf, frame.size.width, frame.size.height);
    return g;
}
static tlValue _window_draw(tlTask* task, tlArgs* args) {
    Window* window = WindowAs(tlArgsTarget(args));
    Graphics* buf = GraphicsCast(tlArgsAt(args, 0));
    if (!buf) TL_THROW("expected a buffer");
    while (a_swap_if(A_VAR(window->draw), A_VAL(buf), null) != null) {
        [[window->nswindow contentView]
                performSelectorOnMainThread: @selector(draw) withObject: nil waitUntilDone: NO];
        usleep(20*1000);
    }
    [[window->nswindow contentView]
            performSelectorOnMainThread: @selector(draw) withObject: nil waitUntilDone: NO];
    return tlNull;
}
static tlValue _window_setTitle(tlTask* task, tlArgs* args) {
    Window* window = WindowAs(tlArgsTarget(args));
    tlText* text = tlTextCast(tlArgsAt(args, 0));
    if (text) {
        [window->nswindow setTitle: [NSString stringWithUTF8String: tlTextData(text)]];
    }
    return tlNull;
}
static tlValue _window_frame(tlTask* task, tlArgs* args) {
    Window* window = WindowAs(tlArgsTarget(args));
    int height = 0;
    NSScreen* screen = [window->nswindow screen];
    if (screen) height = [screen frame].size.height;
    NSRect wframe = [window->nswindow frame];
    NSRect vframe = [[window->nswindow contentView] frame];
    int y = height - wframe.origin.y - vframe.size.height;
    if (y < 0) y = 0;
    return tlResultNewFrom(task,
            tlINT(wframe.origin.x), tlINT(y),
            tlINT(vframe.size.width), tlINT(vframe.size.height), null);
}
static tlValue _window_mouse(tlTask* task, tlArgs* args) {
    Window* window = WindowAs(tlArgsTarget(args));
    int height = 0;
    NSScreen* screen = [window->nswindow screen];
    if (screen) height = [screen frame].size.height;
    NSPoint point = [NSEvent mouseLocation];
    return tlResultNewFrom(task, tlINT(point.x), tlINT(height - point.y), null);
}

@implementation HotelWindow
- (void)keyDown: (NSEvent*)event {
    NSLog(@"keydown: %@", event);
}
@end

@implementation HotelView

-(void)mouseDown:(NSEvent*)event {
    NSLog(@"mouseDown: %@", event);
}

- (void)draw {
    [self setNeedsDisplay: YES];
}

- (void)drawRect: (NSRect)rect {
    Graphics* buf = A_PTR(a_swap(A_VAR(window->draw), null));
    if (buf) {
        if (window->old) {
            if (a_swap_if(A_VAR(window->buf), A_VAL(window->old), null) != null) {
                print(">> BUFFER FULL: DELETING");
                graphicsDelete(window->old);
            }
        }
        window->old = buf;
    } else {
        if (!window->old) return;
        buf = window->old;
    }
    assert(buf);
    NSRect frame = [self frame];

    // get gc context and translate/scale it for cairo
    CGContextRef ctx = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
    CGContextTranslateCTM(ctx, 0.0, frame.size.height);
    CGContextScaleCTM(ctx, 1.0, -1.0);

    // create a cairo context and drow the buf context to it
    cairo_surface_t *surface =
            cairo_quartz_surface_create_for_cg_context(ctx, frame.size.width, frame.size.height);
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
        "setTitle", _window_setTitle,
        "frame", _window_frame,
        "mouse", _window_mouse,
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

// can be called many times, will once init the Cocoa framework
static void ns_init() {
    static bool inited;
    if (inited) return; inited = true;

    pthread_mutex_lock(&cocoa);
    should_start_cocoa = true;
    pthread_cond_signal(&cocoa_start);
    pthread_cond_wait(&cocoa_start, &cocoa);
    pthread_mutex_unlock(&cocoa);

    shared = tlAlloc(null, AppClass, sizeof(App));
    shared->app = NSApp;
}

// will stop the Cocoa framework if started, otherwise will just exit the cocoa thread
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

// Cocoa cannot be convinced its main thread to be something but the first thread
// so we immediately launch a second thread for the hotel interpreter
// TODO we can, theoretically, gift cocoa our thread if we control our worker better ...
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

// we launch the hotel interpreter thread, and then wait
// if the interpreter wishes to start the Cocoa environment, we let it
int main() {
    pthread_t tl_thread;
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
        // this will only return when Cocoa's mainloop is done
        [NSApp run];
    }

    pthread_join(tl_thread, null);
    return 0;
}

