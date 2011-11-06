#import <Cocoa/Cocoa.h>
#include <cairo/cairo.h>
#include <cairo/cairo-quartz.h>

#include "graphics.h"

void draw(cairo_t* cr) {
    NSLog(@"testing ...");
    test_graphics(cr);
    NSLog(@"done ...");
}

@interface CanvasView: NSView { }
@end

@implementation CanvasView

- (id)initWithFrame: (NSRect)frameRect {
    if ((self = [super initWithFrame:frameRect]) != nil) {
    }
    return self;
}

- (BOOL)isOpaque {
    return NO;
}

- (void)drawRect: (NSRect)rect {
    NSRect bounds = [self bounds];
    int width = bounds.size.width;
    int height = bounds.size.height;

    // get gc context and translate/scale it for cairo
    CGContextRef ctx = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
    CGContextTranslateCTM(ctx, 0.0, height);
    CGContextScaleCTM(ctx, 1.0, -1.0);

    // create a cairo context; then push as a group to workaround a cairo quartz bug
    cairo_surface_t *surface = cairo_quartz_surface_create_for_cg_context(ctx, width, height);
    cairo_t *cr = cairo_create(surface);
    cairo_push_group(cr);

    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);
    draw(cr);

    // pop the group, paint it and cleanup
    cairo_pop_group_to_source(cr);
    cairo_paint(cr);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

@end

int main() {
    [NSAutoreleasePool new];
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy: NSApplicationActivationPolicyRegular];

    id appName = [[NSProcessInfo processInfo] processName];

    id menubar = [[NSMenu new] autorelease];
    id appMenuItem = [[NSMenuItem new] autorelease];
    [menubar addItem: appMenuItem];
    [NSApp setMainMenu: menubar];

    id appMenu = [[NSMenu new] autorelease];
    id quitTitle = [@"Quit " stringByAppendingString: appName];
    id quitMenuItem = [[[NSMenuItem alloc] initWithTitle: quitTitle
                action:@selector(terminate:) keyEquivalent:@"q"] autorelease];
    [appMenu addItem: quitMenuItem];
    [appMenuItem setSubmenu: appMenu];

    id window = [[[NSWindow alloc]
            initWithContentRect: NSMakeRect(0, 0, 200, 200)
                      styleMask: NSResizableWindowMask|NSClosableWindowMask|NSTitledWindowMask
                        backing: NSBackingStoreBuffered
                          defer: NO] autorelease];
    [window setContentView: [[CanvasView new] autorelease]];
    [window makeKeyAndOrderFront: nil];

    [NSApp activateIgnoringOtherApps: YES];
    [NSApp run];
    return 0;
}

