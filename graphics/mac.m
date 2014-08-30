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
    NSRect restoreRect;
} @end
@interface NView: NSView {
@public
    Window* window;
} @end
@interface NTextView : NSScrollView {
    NSTextView *textView;
} @end

NativeWindow* nativeWindowNew(Window* window) {
    // TODO use or update x and y
    NWindow* w = [[NWindow alloc]
            initWithContentRect: NSMakeRect(0, 0, window->width, window->height)
                      styleMask: NSResizableWindowMask|NSClosableWindowMask|NSTitledWindowMask
                        backing: NSBackingStoreBuffered
                          defer: NO];
    [w setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
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
bool nativeWindowFullScreen(NativeWindow* _w) {
    NWindow* w = (NWindow*)_w;
    return ([w styleMask] & NSFullScreenWindowMask) == NSFullScreenWindowMask;
}
void nativeWindowSetFullScreen(NativeWindow* _w, bool full) {
    trace("toggleFullScreen: %p", _w);
    if (nativeWindowFullScreen(_w) == full) return;
    NWindow* w = (NWindow*)_w;
    [w toggleFullScreen:nil];
}

@implementation NWindow
- (void)keyDown: (NSEvent*)event {
    int key = [event keyCode];
    const char* input = [[event characters] UTF8String];
    trace("HAVE A KEY: %d %s", key, input);
    // translate cocoa events into html keycodes (and therefor VK codes like java and .net)
    switch (key) {
        case 36: key = 13; input = ""; break; // enter
        case 51: key = 8; input = ""; break; // backspace
        case 117: key = 127; input = ""; break; // delete
        case 53: key = 27; input = ""; break; // esc

        case 123: key = 37; input = ""; break; // cursors
        case 124: key = 39; input = ""; break;
        case 125: key = 40; input = ""; break;
        case 126: key = 38; input = ""; break;

        case 115: key = 36; input = ""; break; // begin
        case 119: key = 35; input = ""; break; // end
        case 116: key = 33; input = ""; break; // pgup
        case 121: key = 34; input = ""; break; // pgdown

        // crazy way to convert from 'a'/'A' to 65 and other chars
        default: key = [[[event charactersIgnoringModifiers] uppercaseString] characterAtIndex: 0];
    }
    if (input[0]) {
        switch (key) {
            case '(': key = 57; break;
            case ')': key = 48; break;
            case '{':
            case '[': key = 219; break;
            case '}':
            case ']': key = 221; break;
            case '\'':
            case '"': key = 222; break;
            case '!': key = 49; break;
            case '@': key = 50; break;
            case '#': key = 51; break;
            case '$': key = 52; break;
            case '%': key = 53; break;
            case '^': key = 54; break;
            case '&': key = 55; break;
            case '*': key = 56; break;
            case '-': key = 45; break;
            case '_': key = 45; break;
            case '=': key = 43; break;
            case '+': key = 43; break;
        }
    }
    int modifiers = 0;
    int m = [event modifierFlags];
    if (m & NSShiftKeyMask) modifiers |= 1;   // shift
    if (m & NSCommandKeyMask) modifiers |= 2; // ctrl
    windowKeyEvent(window, key, tlStringFromCopy(input, 0), modifiers);
}
- (void)windowWillClose:(NSNotification *)notification {
    closeWindow(window);
}
- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)frameSize {
    NSRect rect = [self contentRectForFrameRect:NSMakeRect(0, 0, frameSize.width, frameSize.height)];
    windowResizeEvent(window, 0, 0, rect.size.width, rect.size.height);
    return frameSize;
}
- (void)windowWillExitFullScreen:(NSNotification *)notification {
    NSRect rect = restoreRect;
    windowResizeEvent(window, rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
}
- (void)windowWillEnterFullScreen:(NSNotification *)notification {
    restoreRect = [self contentRectForFrameRect:[self frame]];
}
@end

@implementation NView
- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)becomeFirstResponder { return YES; }

- (void)drawRect: (NSRect)rect {
    [super drawRect:rect];
    if (!window) return;

    int width = self.bounds.size.width;
    int height = self.bounds.size.height;

    CGContextRef cgx = [[NSGraphicsContext currentContext] graphicsPort];
    //use when not using isFlipped:YES
    //CGContextTranslateCTM(cgx, 0.0, height);
    //CGContextScaleCTM(cgx, 1.0, -1.0);
    cairo_surface_t* surface = cairo_quartz_surface_create_for_cg_context(cgx, width, height);
    cairo_t* cairo = cairo_create(surface);

    renderWindow(window, cairo);

    cairo_destroy(cairo);
    cairo_surface_destroy(surface);
}
static int buttons = 0;
- (void)rightMouseDown:(NSEvent*)event { [self mouseDown:event]; }
- (void)otherMouseDown:(NSEvent*)event { [self mouseDown:event]; }
- (void)mouseDown:(NSEvent*)event {
    //NSLog(@"mouseDown: %@", event);
    NSPoint loc = [event locationInWindow];
    switch ([event type]) {
        case NSLeftMouseDown:  buttons |= 1; break;
        case NSRightMouseDown: buttons |= 2; break;
        case NSOtherMouseDown: buttons |= 4; break;
    }
    int count = [event clickCount];
    int modifiers = 0;
    int m = [event modifierFlags];
    if (m & NSShiftKeyMask) modifiers |= 1;   // shift
    if (m & NSCommandKeyMask) modifiers |= 2; // ctrl
    windowMouseEvent(window, loc.x, self.bounds.size.height - loc.y, buttons, count, modifiers);
}
- (void)rightMouseUp:(NSEvent*)event { [self mouseUp:event]; }
- (void)otherMouseUp:(NSEvent*)event { [self mouseUp:event]; }
- (void)mouseUp:(NSEvent*)event {
    //NSLog(@"mouseUp: %@", event);
    switch ([event type]) {
        case NSLeftMouseUp:  buttons &= ~1; break;
        case NSRightMouseUp: buttons &= ~2; break;
        case NSOtherMouseUp: buttons &= ~4; break;
    }
    /*
    NSPoint loc = [event locationInWindow];
    int count = [event clickCount];
    int modifiers = 0;
    int m = [event modifierFlags];
    if (m & NSShiftKeyMask) modifiers |= 1;   // shift
    if (m & NSCommandKeyMask) modifiers |= 2; // ctrl
    windowMouseEvent(window, loc.x, self.bounds.size.height - loc.y, buttons, modifiers, count);
    */
}
- (void)rightMouseDragged:(NSEvent*)event { [self mouseDragged:event]; }
- (void)otherMouseDragged:(NSEvent*)event { [self mouseDragged:event]; }
- (void)mouseDragged:(NSEvent*)event {
    NSPoint loc = [event locationInWindow];
    windowMouseMoveEvent(window, loc.x, self.bounds.size.height - loc.y, buttons, 0);
}

- (void)scrollWheel:(NSEvent*)event {
    windowMouseScrollEvent(window, [event deltaX], [event deltaY]);
}
@end

@implementation NTextView
- (id)initWithFrame: (NSRect)frame {
    if ((self = [super initWithFrame: frame])) {
        textView = [[NSTextView alloc] initWithFrame: NSMakeRect(0, 0, frame.size.width, frame.size.height)];
        [textView setVerticallyResizable: YES];
        [textView setHorizontallyResizable: YES];
        [self setBorderType: NSBezelBorder];
        [self setHasVerticalScroller: YES];
        [self setHasHorizontalScroller: NO];
        [self setDocumentView: textView];
        //[textView setDelegate: self];
    }
    [textView setString:@"hello world!"];
    return self;
}
-(tlString*)text {
    return tlStringFromCopy([[[textView textStorage] string] UTF8String], 0);
}
-(void)setText:(tlString*)text {
    [textView setString:[NSString stringWithUTF8String:tlStringData(text)]];
}
-(void)textDidChange: (NSNotification *)notification {
    // TODO implement
}
@end

NativeTextBox* nativeTextBoxNew(NativeWindow* _w, int x, int y, int width, int height) {
    NWindow* window = (NWindow*)_w;
    NTextView* text = [[NTextView alloc] initWithFrame:NSMakeRect(x, y, width, height)];
    [window.contentView addSubview:text];
    return text;
}
void nativeTextBoxPosition(NativeTextBox* _text, int x, int y, int width, int height) {
    NTextView* view = (NTextView*)_text;
    [view setFrame:NSMakeRect(x, y, width, height)];
}
void nativeTextBoxSetText(NativeTextBox* _text, tlString* string) {
    NTextView* view = (NTextView*)_text;
    [view setText:string];
}
tlString* nativeTextBoxGetText(NativeTextBox* _text) {
    NTextView* view = (NTextView*)_text;
    return [view text];
}

tlString* nativeClipboardGet() {
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSArray *classes = [[NSArray alloc] initWithObjects:[NSString class], nil];
    NSDictionary *options = [NSDictionary dictionary];
    NSArray *copiedItems = [pasteboard readObjectsForClasses:classes options:options];
    if (copiedItems == nil || [copiedItems count] == 0) return tlStringEmpty();
    NSString* str = [copiedItems objectAtIndex: 0];
    return tlStringFromCopy([str UTF8String], 0);
}

void nativeClipboardSet(tlString* str) {
    NSPasteboard *pasteBoard = [NSPasteboard generalPasteboard];
    [pasteBoard declareTypes:[NSArray arrayWithObjects:NSStringPboardType, nil] owner:nil];
    [pasteBoard setString:[NSString stringWithUTF8String:tlStringData(str)] forType:NSStringPboardType];
}

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

