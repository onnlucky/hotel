#include <math.h>

#include "graphics.h"
#include "../tl.h"

struct Graphics {
    tlHead head;
    cairo_t* cairo;
};

static tlClass _GraphicsClass;
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

Graphics* graphicsSizeTo(tlTask* task, Graphics* g, int width, int height) {
    if (!g) {
        g = tlAlloc(task, GraphicsClass, sizeof(Graphics));
    }
    return g;
}
void graphicsDrawOn(Graphics* g, cairo_t* cr) {
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

