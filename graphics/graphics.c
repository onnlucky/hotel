#include <math.h>

#include "graphics.h"
#include "../tl.h"

struct Graphics {
    tlHead head;
    cairo_surface_t* surface;
    cairo_t* cairo;
};

static tlClass _GraphicsClass = {
    .name = "Graphics"
};
tlClass* GraphicsClass = &_GraphicsClass;

static tlValue _circle(tlTask* task, tlArgs* args) {
    print("!! HERE !!");
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_t* cr = g->cairo;
    int x = tl_int_or(tlArgsAt(args, 0), 0);
    int y = tl_int_or(tlArgsAt(args, 1), 0);
    int r = tl_int_or(tlArgsAt(args, 2), -1);
    if (r <= 0) return tlNull;
    cairo_arc(cr, x, y, r, 0, 2*M_PI);
    cairo_fill_preserve(cr);
    cairo_stroke(cr);
    return tlNull;
}

static tlValue _rgb(tlTask* task, tlArgs* args) {
    Graphics* gr = GraphicsAs(tlArgsTarget(args));
    cairo_t* cr = gr->cairo;
    float r = tl_int_or(tlArgsAt(args, 0), 0)/255.0;
    float g = tl_int_or(tlArgsAt(args, 1), 0)/255.0;
    float b = tl_int_or(tlArgsAt(args, 2), 0)/255.0;
    cairo_set_source_rgb(cr, r, g, b);
    return tlNull;
}

void graphicsDelete(Graphics* g) {
    // TODO this is not thread save ... so we must guarentee that somehow
    if (g->cairo) { cairo_destroy(g->cairo); g->cairo = null; }
    if (g->surface) { cairo_surface_destroy(g->surface); g->surface = null; }
}

Graphics* graphicsSizeTo(tlTask* task, Graphics* g, int width, int height) {
    print("CREATING GRAPHICS!!! width: %d, height: %d", width, height);
    if (!g) {
        g = tlAlloc(task, GraphicsClass, sizeof(Graphics));
    }
    if (!g->surface || cairo_image_surface_get_width(g->surface) != width
            || cairo_image_surface_get_height(g->surface) != height) {
        if (g->cairo) { cairo_destroy(g->cairo); g->cairo = null; }
        if (g->surface) { cairo_surface_destroy(g->surface); g->surface = null; }
        g->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
        g->cairo = cairo_create(g->surface);
    }
    return g;
}
void graphicsDrawOn(Graphics* g, cairo_t* cr) {
    print("DRAWING!!!");
    cairo_set_source_surface(cr, g->surface, 0, 0);
    cairo_fill_extents(cr, null, null, null, null);
}

static tlValue _Graphics_new(tlTask* task, tlArgs* args) {
    return graphicsSizeTo(task, null, 0, 0);
}

void graphics_init(tlVm* vm) {
    _GraphicsClass.map = tlClassMapFrom(
        "rgb", _rgb,
        "circle", _circle,
        null
    );
    tlMap* GraphicsStatic = tlClassMapFrom(
        "new", _Graphics_new,
        null
    );
    tlVmGlobalSet(vm, tlSYM("Graphics"), GraphicsStatic);
}

