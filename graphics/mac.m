#import <Cocoa/Cocoa.h>
#include <pthread.h>
#include <cairo/cairo.h>
#include <cairo/cairo-quartz.h>

#include "vm/tl.h"
#include "graphics.h"
#include "image.h"
#include "window.h"
#include "app.h"

// an implementation of how to integrate the cocoa framework with the hotelvm
// There is a slight gotcha:
// when the cocoa mainthread wishes to render, we use a tlBlockingTask, this will run on "a" hotel thread
// however, that same thread might be calling into the cocoa framework, which internally also uses locks
// leading to deadlock (of sorts)
//
// Two solutions:
// 1. never block hotel threads by doing all cocoa work on the cocoa main thread (attempted now)
// 2. take a hotel level lock on the window object, blocking until we have it, then do our work

@interface NWindow: NSWindow {
@public
    Window* window;
} @end
@interface NView: NSView {
@public
    Window* window;
} @end

NativeWindow* nativeWindowNew(Window* window) {
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
    [w close];
}
void nativeWindowFocus(NativeWindow* _w) {
    NWindow* w = (NWindow*)_w;
    [NSApp activateIgnoringOtherApps: YES];
    [w makeKeyAndOrderFront:nil];
}
void nativeWindowSetVisible(NativeWindow* _w, bool visible) {
    NWindow* w = (NWindow*)_w;
    // cocoa is always weird about show/hide of windows and such ...
    if (visible) {
        [w orderFront:nil];
    } else {
        [w orderOut:nil];
    }
}
int nativeWindowVisible(NativeWindow* _w) {
    NWindow* w = (NWindow*)_w;
    return [w isVisible];
}
void nativeWindowRedraw(NativeWindow* _w) {
    NWindow* w = (NWindow*)_w;
    [[w contentView] performSelectorOnMainThread:@selector(setNeedsDisplay:) withObject:w waitUntilDone:NO];
}

void nativeWindowSetTitle(NativeWindow* _w, tlString* title) {
    NWindow* w = (NWindow*)_w;
    if (title) [w setTitle:[NSString stringWithUTF8String: tlStringData(title)]];
}
tlString* nativeWindowTitle(NativeWindow* _w) {
    NWindow* w = (NWindow*)_w;
    return tlStringFromCopy([[w title] UTF8String], 0);
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
    windowKeyEvent(window, key, tlStringFromCopy(input, 0));
}
- (void)windowWillClose:(NSNotification *)notification {
    closeWindow(window);
}
@end

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

// **** connect to app.h ****

@interface Signal : NSObject
@end
@implementation Signal
- (void)run { toolkit_started(); }
@end

void toolkit_init(int argc, char** argv) {
    // no init needed
}

void toolkit_start() {
    [NSAutoreleasePool new];
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy: NSApplicationActivationPolicyRegular];
    //[NSApp setActivationPolicy: NSApplicationActivationPolicyAccessory];
    NSThread* dummy = [[[NSThread alloc] init] autorelease]; [dummy start];
    assert([NSThread isMainThread]);
    assert([NSThread isMultiThreaded]);
    Signal* signal = [[Signal alloc] init];
    [signal performSelectorOnMainThread:@selector(run) withObject:nil waitUntilDone:NO];
    [NSApp run];
}

void toolkit_stop() {
    // TODO this fixes the above stop waiting until first event in queue actually stops the loop
    // we need to post an event in the queue and wake it up
    [[NSApplication sharedApplication] performSelectorOnMainThread: @selector(stop:) withObject: nil waitUntilDone: NO];
    [[NSApplication sharedApplication] performSelectorOnMainThread: @selector(terminate:) withObject: nil waitUntilDone: NO];
}

@interface RunOnMain : NSObject {
    @public
    tlRunOnMain* onmain;
}
@end
@implementation RunOnMain
- (void)run {
    onmain->result = onmain->cb(onmain->args);
    toolkit_schedule_done(onmain);
}
@end
void toolkit_schedule(tlRunOnMain* onmain) {
    RunOnMain* run = [[RunOnMain alloc] init];
    run->onmain = onmain;
    [run performSelectorOnMainThread:@selector(run) withObject:nil waitUntilDone:NO];
}

