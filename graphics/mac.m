#import <Cocoa/Cocoa.h>
#include <pthread.h>
#include <cairo/cairo.h>
#include <cairo/cairo-quartz.h>

#include "vm/tl.h"
#include "graphics.h"
#include "window.h"

// an implementation of how to integrate the cocoa framework with the hotelvm
// There is a slight gotcha:
// when the cocoa mainthread wishes to render, we use a tlBlockingTask, this will run on "a" hotel thread
// however, that same thread might be calling into the cocoa framework, which internally also uses locks
// leading to deadlock (of sorts)
//
// Two solutions:
// 1. never block hotel threads by doing all cocoa work on the cocoa main thread (attempted now)
// 2. take a hotel level lock on the window object, blocking until we have it, then do our work

static void ns_init();

TL_REF_TYPE(App);
struct App {
    tlHead head;
    NSApplication* app;
};
static tlKind _AppKind = { .name = "App", };
tlKind* AppKind = &_AppKind;
static App* shared;

static tlHandle App_shared(tlArgs* args) {
    ns_init();
    assert(shared);
    return shared;
}

@interface NWindow: NSWindow {
@public
    Window* window;
} @end
@interface NView: NSView {
@public
    Window* window;
} @end

NativeWindow* nativeWindowNew(Window* window) {
    ns_init();
    // TODO use or update x and y
    NWindow* w = [[NWindow alloc]
            initWithContentRect: NSMakeRect(0, 0, window->width, window->height)
                      styleMask: NSResizableWindowMask|NSClosableWindowMask|NSTitledWindowMask
                        backing: NSBackingStoreBuffered
                          defer: NO];
    [w setDelegate: (id)w];
    NView* v = [[NView new] autorelease];
    [w setContentView: v];
    [w center];

    w->window = v->window = window;
    return w;
}
void nativeWindowClose(NativeWindow* _w) {
    NWindow* w = (NWindow*)_w;
    [w performSelectorOnMainThread: @selector(close) withObject:nil waitUntilDone:NO];
}
void nativeWindowFocus(NativeWindow* _w) {
    NWindow* w = (NWindow*)_w;
    [NSApp activateIgnoringOtherApps: YES];
    [w performSelectorOnMainThread: @selector(makeKeyAndOrderFront:) withObject:w waitUntilDone:NO];
}
void nativeWindowSetVisible(NativeWindow* _w, bool visible) {
    NWindow* w = (NWindow*)_w;
    // cocoa is always weird about show/hide of windows and such ...
    if (visible) {
        [w performSelectorOnMainThread: @selector(orderFront:) withObject:w waitUntilDone:NO];
    } else {
        [w performSelectorOnMainThread: @selector(orderOut:) withObject:w waitUntilDone:NO];
    }
}
int nativeWindowVisible(NativeWindow* _w) {
    NWindow* w = (NWindow*)_w;
    return [w isVisible];
}
void nativeWindowRedraw(NativeWindow* _w) {
    NWindow* w = (NWindow*)_w;
    [[w contentView] setNeedsDisplay: YES];
}

void nativeWindowSetTitle(NativeWindow* _w, tlText* title) {
    NWindow* w = (NWindow*)_w;
    if (title) [w performSelectorOnMainThread:@selector(setTitle:) withObject:[NSString stringWithUTF8String: tlTextData(title)] waitUntilDone:NO];
}
tlText* nativeWindowTitle(NativeWindow* _w) {
    NWindow* w = (NWindow*)_w;
    return tlTextFromCopy([[w title] UTF8String], 0);
}

void nativeWindowFrame(NativeWindow* _w, int* x, int* y, int* width, int* height) {
    NWindow* w = (NWindow*)_w;
    NSRect rect = [w frame];
    if (x) *x = rect.origin.x;
    if (y) *y = rect.origin.y;
    if (width) *width = rect.size.width;
    if (height) *height = rect.size.height;
}
void nativeWindowSetPos(NativeWindow* _w, int x, int y) {
    //NWindow* w = (NWindow*)_w;
}
void nativeWindowSetSize(NativeWindow* _w, int width, int height) {
    //NWindow* w = (NWindow*)_w;
}

@implementation NWindow
- (void)keyDown: (NSEvent*)event {
    int key = [event keyCode];
    const char* input = [[event characters] UTF8String];
    trace("HAVE A KEY: %d %s", key, input);
    // translate cocoa events into html keycodes (and therefor VK codes like java and .net)
    switch (key) {
        case 36: key = 13; input = ""; break;
        case 51: key = 8; input = ""; break;
        case 53: key = 27; input = ""; break;
        case 123: key = 37; input = ""; break;
        case 124: key = 39; input = ""; break;
        case 125: key = 40; input = ""; break;
        case 126: key = 38; input = ""; break;
        // crazy way to convert from 'a'/'A' to 65
        default: key = [[[event charactersIgnoringModifiers] uppercaseString] characterAtIndex: 0];
    }
    windowKeyEvent(window, key, tlTextFromCopy(input, 0));
}
- (void)windowWillClose:(NSNotification *)notification {
    //NSLog(@"close %@", notification);
}
@end

static void needsdisplay(void* data) {
    [(NView*)data setNeedsDisplay: YES];
}

@implementation NView
- (void)drawRect: (NSRect)rect {
    if (!window) return;

    int width = self.bounds.size.width;
    int height = self.bounds.size.height;

    CGContextRef cgx = [[NSGraphicsContext currentContext] graphicsPort];
    CGContextTranslateCTM(cgx, 0.0, height);
    CGContextScaleCTM(cgx, 1.0, -1.0);
    cairo_surface_t* surface = cairo_quartz_surface_create_for_cg_context(cgx, width, height);
    cairo_t* cairo = cairo_create(surface);

    renderWindow(window, cairo);

    cairo_destroy(cairo);
    cairo_surface_destroy(surface);
}
-(void)mouseDown:(NSEvent*)event {
    NSLog(@"mouseDown: %@", event);
}
@end


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
    } else {
        pthread_cond_signal(&cocoa_start);
    }
    pthread_mutex_unlock(&cocoa);
}

static int vm_exit_code = 0;

// Cocoa cannot be convinced its main thread to be something but the first thread
// so we immediately launch a second thread for the hotel interpreter
// TODO we can, theoretically, gift cocoa our thread if we control our worker better ...
static void* tl_main(void* data) {
    tlVm* vm = tlVmNew();
    tlVmInitDefaultEnv(vm);
    graphics_init(vm);
    window_init(vm);
    tlArgs* args = tlArgsNew(tlListFrom(tlTEXT("run.tl"), null), null);
    tlVmEvalBoot(vm, args);
    vm_exit_code = tlVmExitCode(vm);
    ns_stop();
    return null;
}

// we launch the hotel interpreter thread, and then wait
// if the interpreter wishes to start the Cocoa environment, we let it
int main() {
    tl_init();

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
        NSThread* dummy = [[[NSThread alloc] init] autorelease]; [dummy start];
        assert([NSThread isMainThread]);
        assert([NSThread isMultiThreaded]);
        pthread_cond_signal(&cocoa_start);
        // this will only return when Cocoa's mainloop is done
        [NSApp run];
    }

    pthread_join(tl_thread, null);
    return vm_exit_code;
}

