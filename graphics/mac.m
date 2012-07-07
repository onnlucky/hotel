#import <Cocoa/Cocoa.h>
#include <pthread.h>
#include <cairo/cairo.h>
#include <cairo/cairo-quartz.h>

#include "vm/tl.h"
#include "graphics.h"

static void ns_init();

TL_REF_TYPE(App);
TL_REF_TYPE(Window);

@interface GraphicsView: NSView {
@public
    Window* window;
} @end
@interface GraphicsWindow: NSWindow {
@public
    Window* window;
} @end

struct App {
    tlHead head;
    NSApplication* app;
};
static tlKind _AppKind = {
    .name = "App",
};
tlKind* AppKind = &_AppKind;
static App* shared;

static tlHandle App_shared(tlArgs* args) {
    ns_init();
    assert(shared);
    return shared;
}

struct Window {
    tlHead head;
    GraphicsWindow* nswindow;
    Graphics* g;
};
static tlKind _WindowKind = {
    .name = "Window"
};
tlKind* WindowKind = &_WindowKind;

static tlHandle _Window_new(tlArgs* args) {
    ns_init();
    Window* window = tlAlloc(WindowKind, sizeof(Window));
    window->nswindow = [[GraphicsWindow alloc]
            initWithContentRect: NSMakeRect(0, 0, 200, 200)
                      styleMask: NSResizableWindowMask|NSClosableWindowMask|NSTitledWindowMask
                        backing: NSBackingStoreBuffered
                          defer: NO];
    GraphicsView* view = [[GraphicsView new] autorelease];
    view->window = window;
    [window->nswindow setContentView: view];
    [window->nswindow center];
    window->nswindow->window = window;
    return window;
}

static tlHandle _window_show(tlArgs* args) {
    Window* window = WindowAs(tlArgsTarget(args));
    [window->nswindow makeKeyAndOrderFront: nil];
    return tlNull;
}
static tlHandle _window_hide(tlArgs* args) {
    Window* window = WindowAs(tlArgsTarget(args));
    [window->nswindow orderOut: nil];
    return tlNull;
}
static tlHandle _window_graphics(tlArgs* args) {
    Window* window = WindowAs(tlArgsTarget(args));
    NSRect frame = [[window->nswindow contentView] frame];
    if (window->g) {
        graphicsResize(window->g, frame.size.width, frame.size.height);
        return window->g;
    }
    return window->g = GraphicsNew(frame.size.width, frame.size.height);
}
static tlHandle _window_draw(tlArgs* args) {
    Window* window = WindowAs(tlArgsTarget(args));
    [[window->nswindow contentView]
            performSelectorOnMainThread: @selector(draw) withObject: nil waitUntilDone: NO];
    return tlNull;
}
static tlHandle _window_setTitle(tlArgs* args) {
    Window* window = WindowAs(tlArgsTarget(args));
    tlText* text = tlTextCast(tlArgsGet(args, 0));
    if (text) {
        [window->nswindow setTitle: [NSString stringWithUTF8String: tlTextData(text)]];
    }
    return tlNull;
}
static tlHandle _window_frame(tlArgs* args) {
    Window* window = WindowAs(tlArgsTarget(args));
    int height = 0;
    NSScreen* screen = [window->nswindow screen];
    if (screen) height = [screen frame].size.height;
    NSRect wframe = [window->nswindow frame];
    NSRect vframe = [[window->nswindow contentView] frame];
    int y = height - wframe.origin.y - vframe.size.height;
    if (y < 0) y = 0;
    return tlResultFrom(tlINT(wframe.origin.x), tlINT(y),
            tlINT(vframe.size.width), tlINT(vframe.size.height), null);
}
static tlHandle _window_mouse(tlArgs* args) {
    Window* window = WindowAs(tlArgsTarget(args));
    int height = 0;
    NSScreen* screen = [window->nswindow screen];
    if (screen) height = [screen frame].size.height;
    NSPoint point = [NSEvent mouseLocation];
    return tlResultFrom(tlINT(point.x), tlINT(height - point.y), null);
}

@implementation GraphicsWindow
- (void)keyDown: (NSEvent*)event {
    NSLog(@"keydown: %@", event);
}
@end

@implementation GraphicsView

-(void)mouseDown:(NSEvent*)event {
    NSLog(@"mouseDown: %@", event);
}

- (void)draw {
    [self setNeedsDisplay: YES];
}

static void releaseGrahpicsData(void* info, const void* data, size_t size) {
    // TODO unlock graphics ...
}

- (void)drawRect: (NSRect)rect {
    if (!window->g) return;

    uint8_t* buf;
    int width; int height; int stride;
    graphicsData(window->g, &buf, &width, &height, &stride);

    CGDataProviderRef data = CGDataProviderCreateWithData(
            null, buf, stride * height, releaseGrahpicsData);
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGImageRef img = CGImageCreate(
            width, height, 8, 32, stride,
            colorSpace, kCGImageAlphaFirst|kCGBitmapByteOrder32Little, data,
            NULL, true, kCGRenderingIntentDefault);

    CGContextRef cg = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
    CGContextDrawImage(cg, [self bounds], img);

    CGDataProviderRelease(data);
    CGColorSpaceRelease(colorSpace);
    CGImageRelease(img);
}

@end

void window_init(tlVm* vm) {
    // TODO if we do this, will will need to drain it sometime ...
    [NSAutoreleasePool new];
    _WindowKind.klass = tlClassMapFrom(
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

    shared = tlAlloc(AppKind, sizeof(App));
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
    tlArgs* args = tlArgsNew(tlListFrom(tlTEXT("run.tl"), null), null);
    tlVmEvalBoot(vm, args);
    print("done");
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

