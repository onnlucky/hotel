#include <math.h>

#include "graphics.h"
#include "vm/tl.h"

struct Graphics {
    tlHead head;
    cairo_surface_t* surface;
    cairo_t* cairo;
};

// TODO make locked ...
static tlKind _GraphicsKind = {
    .name = "Graphics"
};
tlKind* GraphicsKind = &_GraphicsKind;

void graphicsResize(Graphics* g, int width, int height) {
    cairo_t* cr = g->cairo;
    cairo_surface_t* surface = g->surface;
    int oldwidth = 0;
    int oldheight = 0;

    if (surface) {
        oldwidth = cairo_image_surface_get_width(surface);
        oldheight = cairo_image_surface_get_height(surface);
        if (oldwidth == width && oldheight == height) return;
    }

    g->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    assert(g->surface);
    g->cairo = cairo_create(g->surface);
    assert(g->cairo);

    if (surface) {
        // TODO copy old pixels to top/left
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
    }
}

void graphicsData(Graphics* g, uint8_t** buf, int* width, int* height, int* stride) {
    if (!g->surface) return;
    if (buf) *buf = cairo_image_surface_get_data(g->surface);
    if (width) *width = cairo_image_surface_get_width(g->surface);
    if (height) *height = cairo_image_surface_get_height(g->surface);
    if (stride) *stride = cairo_image_surface_get_stride(g->surface);
}

void graphicsDelete(Graphics* g) {
    // TODO this is not thread save ... so we must guarentee that somehow
    if (g->cairo) { cairo_destroy(g->cairo); g->cairo = null; }
    if (g->surface) { cairo_surface_destroy(g->surface); g->surface = null; }
}

Graphics* GraphicsNew(int width, int height) {
    trace("new graphics; width: %d, height: %d", width, height);
    Graphics* g = tlAlloc(GraphicsKind, sizeof(Graphics));
    graphicsResize(g, width, height);
    return g;
}

static tlHandle _Graphics_new(tlArgs* args) {
    int width = tl_int_or(tlArgsGet(args, 0), 0);
    int height = tl_int_or(tlArgsGet(args, 1), 0);
    if (width < 0) width = 0;
    if (height < 0) height = 0;

    return GraphicsNew(width, height);
}

static tlHandle _resize(tlArgs* args) {
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    int width = tl_int_or(tlArgsGet(args, 0), 0);
    int height = tl_int_or(tlArgsGet(args, 1), 0);
    if (width < 0) width = 0;
    if (height < 0) height = 0;
    graphicsResize(g, width, height);
    return tlNull;
}

static tlHandle _clear(tlArgs* args) {
    Graphics* g = GraphicsAs(tlArgsTarget(args));

    cairo_surface_flush(g->surface);
    uint8_t* data = cairo_image_surface_get_data(g->surface);
    int stride = cairo_image_surface_get_stride(g->surface);
    int height = cairo_image_surface_get_height(g->surface);
    memset(data, 0, stride * height);
    cairo_surface_mark_dirty(g->surface);
    return tlNull;
}

static tlHandle _circle(tlArgs* args) {
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_t* cr = g->cairo;
    int x = tl_int_or(tlArgsGet(args, 0), 0);
    int y = tl_int_or(tlArgsGet(args, 1), 0);
    int r = tl_int_or(tlArgsGet(args, 2), -1);
    if (r <= 0) return tlNull;
    cairo_arc(cr, x, y, r, 0, 2*M_PI);
    cairo_fill_preserve(cr);
    cairo_stroke(cr);
    return tlNull;
}

static tlHandle _rgb(tlArgs* args) {
    Graphics* gr = GraphicsAs(tlArgsTarget(args));
    cairo_t* cr = gr->cairo;
    float r = tl_int_or(tlArgsGet(args, 0), 0)/255.0;
    float g = tl_int_or(tlArgsGet(args, 1), 0)/255.0;
    float b = tl_int_or(tlArgsGet(args, 2), 0)/255.0;
    cairo_set_source_rgb(cr, r, g, b);
    return tlNull;
}

void graphics_init(tlVm* vm) {
    _GraphicsKind.klass = tlClassMapFrom(
        "resize", _resize,
        "clear", _clear,
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

